#include <iostream>
#include <vector>
#include <queue>
#include "file.hpp"
using namespace std;

//Global variables
int cycle = 0;

queue<Element> ex_wait;
queue<Element> mem_wait;
vector<Element> ex_data_dependencies;
vector<Element> mem_data_dependencies;
int alu_unit = 0;
int float_unit = 0;
int branch_unit = 0;
int L1_read_port = 0;
int L1_write_port = 0;
bool control_hazard = false;
bool data_hazard = false;
int W; //Window size (Value determined by user input)

//Simulated statistics
int num_int = 0;
int num_float = 0;
int num_branch = 0;
int num_loads = 0;
int num_stores = 0;

int main(int argc, char **argv) {
	
	if (argc >= 5) {
		W = atoi(argv[4]);
		int num_instructions = atoi(argv[3]);
		int start_instruction = atoi(argv[2]);
		string file_name = argv[1];
		
		//Create the processor pipeline
		vector<string> s;
		Element Pipeline[W][5];
		for (int i = 0; i < W; i++) {
			for (int j = 0; j < 5; j++) {
				Pipeline[i][j] = Element(0, "", -1, s);
			}
		}

		File file = File(file_name, start_instruction, num_instructions); //Read the instructions from the file
		
		while (true) {			
						
			// Work backwards through the pipeline to process it
			// Pop off the instructions in the last stage and record their type
			for (int i = 0; i < W; i++) {
				Element finished_instruction = Pipeline[i][4];
				if (finished_instruction.type == 1) {
					num_int++;
				} else if (finished_instruction.type == 2) {
					num_float++;
				} else if (finished_instruction.type == 3) {
					num_branch++;
				} else if (finished_instruction.type == 4) {
					num_loads++;
				} else if (finished_instruction.type == 5) {
					num_stores++;
				}
				for (int j = 0; j < ex_data_dependencies.size(); j++) {
					if (finished_instruction.PC == ex_data_dependencies[j].PC) {
						ex_data_dependencies.erase(ex_data_dependencies.begin()+j);
					}
				}
				for (int j = 0; j < mem_data_dependencies.size(); j++) {
					if (finished_instruction.PC == mem_data_dependencies[j].PC) {
						mem_data_dependencies.erase(mem_data_dependencies.begin()+j);
					}
				}
			}
			// Assume that the instructions in the MEM stage move to the WB stage without any structural hazard stalls
			for (int i = 0; i < W; i++) {
				Pipeline[i][4] = Pipeline[i][3];
				Pipeline[i][3] = Element(0, "", -1, s);
			}
			// MEM ports are now free
			L1_read_port = 0;
			L1_write_port = 0;
			// Process the instructions at the EX stage into the MEM stage
			// First check the wait list for the MEM stage
			int i = 0;
			bool send_to_wait = false;
			while (mem_wait.size() > 0 && i < W) {
				send_to_wait = true;
				Element next_instruction = mem_wait.front();
				if (next_instruction.type == 4) {
					if (L1_read_port) {
						// If instruction is a read instruction and the read port is busy, pass dummy instructions through to MEM and end the loop
						for (int j = i; j < W; j++) {
							Pipeline[j][3] = Element(0, "", -1, s);
						}
						break;					
					} else {
						// Pass the instruction into the MEM stage and count the read port as busy
						L1_read_port = 1;
						Pipeline[i][3] = next_instruction;
						mem_wait.pop();
					}
				} else if (next_instruction.type == 5) {
					if (L1_write_port) {
						// If instruction is a write instruction and the write port is busy, pass dummy instructions through to MEM and end the loop
						for (int j = i; j < W; j++) {
							Pipeline[j][3] = Element(0, "", -1, s);
						}
						break;					
					} else {
						// Pass the instruction into the MEM stage and count the write port as busy
						L1_write_port = 1;
						Pipeline[i][3] = next_instruction;
						mem_wait.pop();
					}
				} else {
					// If the instruction is not MEM-related, just pass it to the MEM stage
					Pipeline[i][3] = next_instruction;
					mem_wait.pop();
				}
				i++;
			}
			// If there are still instructions left in the wait queue for MEM, add the next instructions to that queue
			if (mem_wait.size() > 0 || send_to_wait) {
				for (i = 0; i < W; i++) {
					// If the instruction is a real instruction, carry it forward
					if (Pipeline[i][2].type != -1) {
						mem_wait.push(Pipeline[i][2]);
						if (Pipeline[i][2].type == 3) {
							control_hazard = false;
						}
					}
					// Replace the previous instructions with dummy instructions
					Pipeline[i][2] = Element(0, "", -1, s);
				}
			} else {
				// Otherwise check for structural hazards before passing the instructions from EX to MEM
				for (i = 0; i < W; i++) {
					if (Pipeline[i][2].type == 4) {
						// Read memory instruction
						if (L1_read_port) {
							// Stall the pipeline; the rest of the instructions go in the wait queue
							for (int j = i; j < W; j++) {
								if (Pipeline[j][2].type != -1) {
									if (Pipeline[j][2].type == 3) {
										control_hazard = false;
									}
									mem_wait.push(Pipeline[j][2]);
								}
							}
							break;
						} else {
							// Set the read port to busy and pass the instruction through
							L1_read_port = 1;
							Pipeline[i][3] = Pipeline[i][2];
							Pipeline[i][2] = Element(0, "", -1, s);
						}
					} else if (Pipeline[i][2].type == 5) {
						// Read memory instruction
						if (L1_write_port) {
							// Stall the pipeline; the rest of the instructions go in the wait queue
							for (int j = i; j < W; j++) {
								if (Pipeline[j][2].type != -1) {
									if (Pipeline[j][2].type == 3) {
										control_hazard = false;
									}
									mem_wait.push(Pipeline[j][2]);
								}
							}
							break;
						} else {
							// Set the write port to busy and pass the instruction through
							L1_write_port = 1;
							Pipeline[i][3] = Pipeline[i][2];
							Pipeline[i][2] = Element(0, "", -1, s);
						}
					} else {
						if (Pipeline[i][2].type == 3) {
							control_hazard = false;
						}
						// Pass the instruction through
						Pipeline[i][3] = Pipeline[i][2];
						Pipeline[i][2] = Element(0, "", -1, s);
					}
				}
			}
			// Once instructions have passed from EX to MEM, set the EX resources to free
			int alu_unit = 0;
			int float_unit = 0;
			int branch_unit = 0;
			
			// Check to see if data dependencies have been satsified
			for (i = 0; i < ex_data_dependencies.size(); i++) {
				for (int j = 0; j < W; j++) {
					for (int k = 3; k < 5; k++) {
						if (Pipeline[j][k].id == ex_data_dependencies[i].id) {
							ex_data_dependencies.erase(ex_data_dependencies.begin() + i);
						}
					}
				}
			}
			for (i = 0; i < mem_data_dependencies.size(); i++) {
				for (int j = 0; j < W; j++) {
						if (Pipeline[j][4].id == mem_data_dependencies[i].id) {
							mem_data_dependencies.erase(mem_data_dependencies.begin() + i);
						}
				}
			}
			if (ex_data_dependencies.size() == 0 && mem_data_dependencies.size() == 0) {
				data_hazard = false;
			}		
			
			// Check if dependency is in parallel with its dependent
			for (i = 0; i < W; i++) {
				for (int j = 0; j < W; j++) {
					if (j != i) {
						for (int k = 0; k < Pipeline[i][1].hex.size(); k++) {
							if (Pipeline[j][1].PC == Pipeline[i][1].hex[k]) {
								ex_wait.push(Pipeline[j][1]);
								Pipeline[j][1] = Element(0, "", -1, s);
							}
						}
					}
				}
			}
			
			if (!data_hazard) {
				// Then process the ID instructions into the EX stage
				// First check the wait list for the EX stage
				i = 0;
				send_to_wait = false;
				while (ex_wait.size() > 0 && i < W) {
					send_to_wait = true;
					Element next_instruction = ex_wait.front();
					if (next_instruction.type == 1) {
						if (alu_unit) {
							// If instruction is an integer instruction and the integer ALU is busy, pass dummy instructions through to EX and end the loop
							for (int j = i; j < W; j++) {
								Pipeline[j][2] = Element(0, "", -1, s);
							}
							break;					
						} else {
							// Pass the instruction into the EX stage and count the integer ALU as busy
							alu_unit = 1;
							Pipeline[i][2] = next_instruction;
							ex_wait.pop();
						}
					} else if (next_instruction.type == 2) {
						if (float_unit) {
							// If instruction is a floating point instruction and the float unit is busy, pass dummy instructions through to EX and end the loop
							for (int j = i; j < W; j++) {
								Pipeline[j][2] = Element(0, "", -1, s);
							}
							break;					
						} else {
							// Pass the instruction into the EX stage and count the float unit as busy
							float_unit = 1;
							Pipeline[i][2] = next_instruction;
							ex_wait.pop();
						}
					} else if (next_instruction.type == 3) {
						if (branch_unit) {
							// If instruction is a branch instruction and the branch unit is busy, pass dummy instructions through to EX and end the loop
							for (int j = i; j < W; j++) {
								Pipeline[j][2] = Element(0, "", -1, s);
							}
							break;					
						} else {
							// Pass the instruction into the EX stage and count the float unit as busy
							branch_unit = 1;
							Pipeline[i][2] = next_instruction;
							ex_wait.pop();
						}
					} else {
						// If the instruction is not EX-related, just pass it to the EX stage
						Pipeline[i][2] = next_instruction;
						ex_wait.pop();
					}
					i++;
				}
				// If there are still instructions left in the wait queue for EX, add the next instructions from ID to that queue
				if (ex_wait.size() > 0 || send_to_wait) {
					for (i = 0; i < W; i++) {
						// If the instruction is a real instruction, carry it forward
						if (Pipeline[i][1].type != -1) {
							ex_wait.push(Pipeline[i][1]);
						}
						// Replace the previous instructions with dummy instructions
						Pipeline[i][1] = Element(0, "", -1, s);
						
					}
				} else {
					// Otherwise check for structural hazards before passing the instructions from ID to EX
					for (i = 0; i < W; i++) {
						if (Pipeline[i][1].type == 1) {
							// Integer instruction
							if (alu_unit) {
								// Stall the pipeline; the rest of the instructions go in the wait queue
								for (int j = i; j < W; j++) {
									if (Pipeline[j][1].type != -1) {
										ex_wait.push(Pipeline[j][1]);
									}
								}
								break;
							} else {
								// Set the integer unit to busy and pass the instruction through
								alu_unit = 1;
								Pipeline[i][2] = Pipeline[i][1];
								Pipeline[i][1] = Element(0, "", -1, s);
							}
						} else if (Pipeline[i][1].type == 2) {
							// Read memory instruction
							if (float_unit) {
								// Stall the pipeline; the rest of the instructions go in the wait queue
								for (int j = i; j < W; j++) {
									if (Pipeline[j][1].type != -1) {
										ex_wait.push(Pipeline[j][1]);
									}
									Pipeline[i][1] = Element(0, "", -1, s);
								}
								break;
							} else {
								// Set the float unit to busy and pass the instruction through
								float_unit = 1;
								Pipeline[i][2] = Pipeline[i][1];
								Pipeline[i][1] = Element(0, "", -1, s);
							}
						} else if (Pipeline[i][1].type == 3) {
							// Branch instruction
							if (branch_unit) {
								// Stall the pipeline; the rest of the instructions go in the wait queue
								for (int j = i; j < W; j++) {
									if (Pipeline[j][1].type != -1) {
										ex_wait.push(Pipeline[j][1]);
									}
									Pipeline[i][1] = Element(0, "", -1, s);
								}
								break;
							} else {
								// Set the branch unit to busy and pass the instruction through
								branch_unit = 1;
								Pipeline[i][2] = Pipeline[i][1];
								Pipeline[i][1] = Element(0, "", -1, s);
							}
						} else {
							// Pass the instruction through
							Pipeline[i][2] = Pipeline[i][1];
							Pipeline[i][1] = Element(0, "", -1, s);
						}
					}
				}
				
				// Pass the instructions from the IF stage to the ID stage. No structural hazards here are possible. 
				for (i = 0; i < W; i++) {
					Pipeline[i][1] = Pipeline[i][0];
					Pipeline[i][0] = Element(0, "", -1, s);
					if (Pipeline[i][1].hex.size() > 0) {
						// Check to see if dependencies are already satisfied, otherwise set flag
						bool satisfied = true;
						for (int k = 0; k < Pipeline[i][1].hex.size(); k++) {
							// For each dependency, check if it has passed its required stage yet. 
							for (int l = 0; l < W; l++) {
								for (int j = 2; j < 5; j++) {
									if (Pipeline[l][j].PC == Pipeline[i][1].hex[k]) {
										if (Pipeline[l][j].type == 1 || Pipeline[l][j].type == 2) {
											// Check if the instruction has passed the EX stage
											if (j < 3) {
												// If not, set flag
												satisfied = false;
												bool contains = false;
												for (int n = 0; n < ex_data_dependencies.size(); n++) {
													if (ex_data_dependencies[n].PC == Pipeline[l][j].PC) {
														contains = true;
														break;
													}
												}
												if (!contains) {
													ex_data_dependencies.push_back(Pipeline[l][j]);
												}
											}
										} else if (Pipeline[l][j].type == 4 || Pipeline[l][j].type == 5) {
											// Check if the instruction has passed the MEM stage
											if (j < 4) {
												// If not, set flag
												satisfied = false;
												bool contains = false;
												for (int n = 0; n < mem_data_dependencies.size(); n++) {
													if (mem_data_dependencies[n].PC == Pipeline[l][j].PC) {
														contains = true;
														break;
													}
												}
												if (!contains) {
													mem_data_dependencies.push_back(Pipeline[l][j]);
												}
											}
										}
									}
								}
							}
						}
						if (!satisfied) {
							data_hazard = true;
						}
					}
				}
				
				// Read in new instructions from the instruction list
				if (!control_hazard) {
					for (i = 0; i < W; i++) {
						if (file.getList().size() > 0) {
							Pipeline[i][0] = file.pop();
							if (Pipeline[i][0].type == 3) {
								control_hazard = true;
								break;
							}
						} 
					}
				} 			
			}
			
			// Print the pipeline
			/*
			printf("%d\n", file.getList().size());
			printf("%d\n", ex_data_dependencies.size());
			printf("%d\n", mem_data_dependencies.size());
			printf("%d\n", control_hazard);
			printf("%d\n", data_hazard);
			printf("%d\n", ex_wait.size());
			printf("%d\n", mem_wait.size());
			for (i = 0; i < W; i++) {
				for (int j = 0; j < 5; j++) {
					printf(" %d ", Pipeline[i][j].type);
				} 
				printf("\n");
			}
			printf("\n");
			*/
			// Check if instruction list is empty and pipeline is empty
			bool pipeline_empty = true;
			for (i = 0; i < W; i++) {
				for (int j = 0; j < 5; j++) {
					if (Pipeline[i][j].type != -1) {
						pipeline_empty = false;
						break;
					}
				}
				if (!pipeline_empty) {
					break;
				}
			}
			if (pipeline_empty && file.getList().size() == 0) {
				// End program
				break;
			}
			
			// Increment the clock by 1 cycle
			cycle++;
		}
		
		printf("Total cycles: %d\n", cycle);
		printf("Total integer instructions: %d\n", num_int);
		printf("Total floating point instructions: %d\n", num_float);
		printf("Total branch instructions: %d\n", num_branch);
		printf("Total memory load instructions: %d\n", num_loads);
		printf("Total memory store instructions: %d\n", num_stores);
		printf("Total instructions: %d\n", num_stores + num_branch + num_float + num_int + num_loads);
		
	} else {
		printf("Insufficient arguments provided.\n");
	}  

    return 0;
}
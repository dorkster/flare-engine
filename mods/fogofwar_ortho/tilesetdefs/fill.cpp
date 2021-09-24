#include <iostream>
#include <fstream>
#include <sstream>

int main() {
	std::string file_name("tileset_fogofwar.txt");
	std::ifstream input(file_name);
	std::stringstream ss;
	std::string line("");
	int skip[512];
	int index = 0;
	int trim_start(0);
	int trim_end(0);
	
	for (int i=0; i<512; i++) skip[i] = 0;
	
	if (input.is_open()) {
		while(std::getline(input, line)) {
			ss << line << std::endl;
			if (line.substr(0, 4) == "tile") {
				trim_start = line.find("=");
				trim_end = line.find(",");				
				line = line.substr(trim_start+1, trim_end-trim_start-1);
				skip[std::stoi(line)] = 1;
				std::cout << line << std::endl;
			}
			else if (line.substr(0,5) == "#stop") {
				std::cout << "stop\n";
				break;
			}
		}		
	}
	
	input.close();
	
	for (int i=0; i<512; i++) {
		if (skip[i] == 0 ) {
			ss << "tile=" << i << ",0,0,64,32,32,16\n";
		}
	}
	
	std::ofstream ofile(file_name);
	ofile << ss.str();
	ofile.close();
}

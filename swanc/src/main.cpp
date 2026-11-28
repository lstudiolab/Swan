#include "swan/codegen.hpp"
#include "swan/lexer.hpp"
#include "swan/parser.hpp"
#include "swan/semantic.hpp"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
int main(int argc,char** argv){bool check=false;std::string output="a.c";std::vector<std::string> inputs;for(int i=1;i<argc;++i){std::string a=argv[i];if(a=="--check")check=true;else if(a=="--emit-c"&&i+1<argc)output=argv[++i];else inputs.push_back(a);}if(inputs.empty()){std::cerr<<"usage: swanc [--check] [--emit-c output.c] file.swan...\n";return 2;}
swan::Module combined;bool failed=false;for(const auto& path:sources){std::ifstream in(path);if(!in){std::cerr<<"swanc: cannot read "<<path<<"\n";failed=true;continue;}std::ostringstream data;data<<in.rdbuf();swan::Lexer lexer(data.str());auto tokens=lexer.scan();for(auto& e:lexer.errors())std::cerr<<path<<": "<<e<<"\n";swan::Parser parser(std::move(tokens));auto module=parser.parse();for(auto& e:parser.errors())std::cerr<<path<<": "<<e<<"\n";if(!lexer.errors().empty()||!parser.errors().empty())failed=true;for(auto& f:module.functions)combined.functions.push_back(std::move(f));}
if(failed)return 1;swan::SemanticAnalyzer semantics;if(!semantics.analyze(combined)){for(const auto& e:semantics.errors())std::cerr<<"swanc: "<<e<<"\\n";return 1;}if(check){std::cout<<"Swan: syntax checks passed ("<<inputs.size()<<" source file(s))\n";return 0;}swan::CEmitter emitter;std::ofstream out(output);if(!out){std::cerr<<"swanc: cannot write "<<output<<"\n";return 2;}out<<emitter.emit(combined);if(!out){std::cerr<<"swanc: failed writing "<<output<<"\n";return 2;}std::cout<<"swanc: emitted "<<output<<" from "<<inputs.size()<<" source file(s)\n";return 0;}

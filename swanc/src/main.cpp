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
int main(int argc, char** argv) {
  bool check = false;
  std::string output = "a.c";
  std::vector<std::string> inputs;
  for (int i=1;i<argc;++i) {
    std::string arg=argv[i];
    if(arg=="--check") check=true;
    else if(arg=="--emit-c" && i+1<argc) output=argv[++i];
    else inputs.push_back(arg);
  }
  if(inputs.empty()) {
    std::cerr<<"usage: swanc [--check] [--emit-c output.c] file.swan|project-directory...\n";
    return 2;
  }
  std::vector<std::string> sources;
  for(const auto& input:inputs) {
    std::error_code ec;
    std::filesystem::path path(input);
    if(std::filesystem::is_directory(path,ec)) {
      for(const auto& entry:std::filesystem::recursive_directory_iterator(path,ec))
        if(entry.is_regular_file() && entry.path().extension()==".swan")
          sources.push_back(entry.path().string());
      if(ec) std::cerr<<"swanc: directory scan warning: "<<ec.message()<<"\n";
    } else sources.push_back(input);
  }
  std::sort(sources.begin(),sources.end());
  if(sources.empty()) { std::cerr<<"swanc: no .swan source files found\n"; return 2; }
  swan::Module combined;
  bool failed=false;
  for(const auto& path:sources) {
    std::ifstream in(path);
    if(!in) { std::cerr<<"swanc: cannot read "<<path<<"\n"; failed=true; continue; }
    std::ostringstream data; data<<in.rdbuf();
    swan::Lexer lexer(data.str()); auto tokens=lexer.scan();
    for(const auto& error:lexer.errors()) std::cerr<<path<<": "<<error<<"\n";
    swan::Parser parser(std::move(tokens)); auto module=parser.parse();
    for(const auto& error:parser.errors()) std::cerr<<path<<": "<<error<<"\n";
    if(!lexer.errors().empty() || !parser.errors().empty()) failed=true;
    for(auto& fn:module.functions) combined.functions.push_back(std::move(fn));
  }
  if(failed) return 1;
  swan::SemanticAnalyzer semantics;
  if(!semantics.analyze(combined)) {
    for(const auto& error:semantics.errors()) std::cerr<<"swanc: "<<error<<"\n";
    return 1;
  }
  if(check) { std::cout<<"Swan: checks passed ("<<sources.size()<<" source file(s))\n"; return 0; }
  swan::CEmitter emitter;
  std::ofstream out(output);
  if(!out) { std::cerr<<"swanc: cannot write "<<output<<"\n"; return 2; }
  out<<emitter.emit(combined);
  if(!out) { std::cerr<<"swanc: failed writing "<<output<<"\n"; return 2; }
  std::cout<<"swanc: emitted "<<output<<" from "<<sources.size()<<" source file(s)\n";
  return 0;
}

#pragma once
#include "swan/Compiler/Compiler.hpp"
#include "swan/Driver/CommandLine.hpp"
#include "swan/Support/FileSystem.hpp"
#include "swan/Basic/Diagnostic.hpp"
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <system_error>
#ifdef _WIN32
#include <process.h>
#else
#include <sys/wait.h>
#endif

namespace swan::driver {
inline std::string shellQuote(const std::string& value){
#ifdef _WIN32
    std::string q="\"";for(char c:value){if(c=='\"')q+="\\\\";q+=c;}return q+"\"";
#else
    std::string q="'";for(char c:value){if(c=='\'')q+="'\\''";else q+=c;}return q+"'";
#endif
}
inline int exitCode(int status){
#ifdef _WIN32
    return status;
#else
    if(status==-1)return 1;
    if(WIFEXITED(status))return WEXITSTATUS(status);
    if(WIFSIGNALED(status))return 128+WTERMSIG(status);
    return 1;
#endif
}
inline std::filesystem::path temporaryPath(const std::string& ext){
    auto stamp=std::chrono::steady_clock::now().time_since_epoch().count();
    return std::filesystem::temp_directory_path()/("swan-"+std::to_string(stamp)+ext);
}
inline int assembleAndLink(const std::filesystem::path& assembly,const std::filesystem::path& output,bool object){
    const char* configured=std::getenv("CC");const std::string cc=configured&&*configured?configured:"cc";
    std::string command=shellQuote(cc)+" ";
    command+=object?"-c ":"";
    command+=shellQuote(assembly.string())+" -o "+shellQuote(output.string());
    return exitCode(std::system(command.c_str()));
}
} // namespace swan::driver

namespace swan {
inline int runCompiler(int argc,char** argv){
    driver::CommandLine options;
    if(!driver::parseCommandLine(argc,argv,options)){driver::printHelp();return 2;}
    if(options.showVersion){std::cout<<"Swan "<<versionString()<<'\n';return 0;}
    if(options.showHelp){driver::printHelp();return 0;}
    try{
        if(options.input.extension()!=".swan")throw CompileError("input file must use .swan extension");
        std::string source;
        if(!readTextFile(options.input,source))throw CompileError("cannot open '"+options.input.string()+"'");
        const bool assembly=options.emitAssembly||(options.hasOutput&&options.output.extension()==".s");
        const bool object=options.emitObject||(options.hasOutput&&(options.output.extension()==".o"||options.output.extension()==".obj"));
        if(assembly&&object)throw CompileError("choose either --emit-asm or --emit-obj");
        const auto asmText=Compiler().compileAssembly(source,options.target);
        auto output=options.hasOutput?options.output:
            assembly?std::filesystem::path(options.input.stem().string()+".s"):
            object?std::filesystem::path(options.input.stem().string()+".o"):
            options.action==driver::Action::Run?driver::temporaryPath(
#ifdef _WIN32
            ".exe"
#else
            ""
#endif
            ):std::filesystem::path(options.input.stem().string()
#ifdef _WIN32
            +".exe"
#endif
            );
        if(assembly){
            if(!writeTextFile(output,asmText))throw CompileError("cannot write assembly output '"+output.string()+"'");
            std::cout<<"Wrote "<<output.string()<<'\n';return 0;
        }
        auto asmPath=driver::temporaryPath(".s");
        struct Cleanup{std::filesystem::path p;~Cleanup(){std::error_code e;std::filesystem::remove(p,e);}} cleanup{asmPath};
        if(!writeTextFile(asmPath,asmText))throw CompileError("cannot write temporary assembly");
        const int status=driver::assembleAndLink(asmPath,output,object);
        if(status!=0){std::error_code e;std::filesystem::remove(output,e);return status;}
        if(object){std::cout<<"Wrote "<<output.string()<<'\n';return 0;}
        if(options.action==driver::Action::Build){std::cout<<"Built "<<output.string()<<'\n';return 0;}
#ifdef _WIN32
        const auto exe=output.string();const char* args[]={exe.c_str(),nullptr};const int result=_spawnv(_P_WAIT,exe.c_str(),args);
#else
        const int result=driver::exitCode(std::system(driver::shellQuote(output.string()).c_str()));
#endif
        if(!options.hasOutput){std::error_code e;std::filesystem::remove(output,e);}
        return result;
    }catch(const std::exception& e){std::cerr<<"swan: "<<e.what()<<'\n';return 1;}
}
} // namespace swan

#include "tests.hpp"
#include "../src/config.hpp"
#include <string>
#include <sstream>
#include <iostream>

static int test_ingest_config(const std::string& path){
    using namespace cloudbus;
    config::configuration conf;
    std::stringstream ss;
    ss << "[Cloudbus]\n"
        << "[Test Service]\n"
        << "bind=unix:///var/run/test.sock\n"
        << "backend=unix:///var/run/backend.sock\n";
    ss >> conf;
    for(auto& section: conf.sections()){
        if(section.heading == "Cloudbus"){
            if(!section.config.empty())
                return TEST_FAIL;
        } else if (section.heading == "Test Service"){
            if(section.config.empty())
                return TEST_FAIL;
            for(auto&[k,v]: section.config){
                if(k == "bind") {
                    if(v != "unix:///var/run/test.sock")
                        return TEST_FAIL;
                } else if (k == "backend"){
                    if(v != "unix:///var/run/backend.sock")
                        return TEST_FAIL;
                } else return TEST_FAIL;
            }
        } else return TEST_FAIL;
    }
    return TEST_PASS;
}
int main(int argc, char **argv){
    int rc = TEST_PASS;
    #ifdef TEST_SRCDIR
        if(rc = test_ingest_config(std::string(TEST_SRCDIR).append("/data/config.ini")))
            return rc;
    #else
        return TEST_ERROR;
    #endif
    return rc;
}
#include "tests.hpp"
#include "../src/config.hpp"
#include <string>
#include <fstream>
#include <iostream>

static int test_ingest_config(const std::string& path){
    using namespace cloudbus;
    if(std::fstream f{path, f.in}){
        config::configuration conf;
        f >> conf;
        return TEST_PASS;
    }
    std::cerr << "Invalid configuration file in test_ingest_config()." << std::endl;
    return TEST_ERROR;
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
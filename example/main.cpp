//===------------------------------ example/main.cpp - [dthread-pool] -----------------------------------*- C++ -*-===//
// Brief :
//
//
// Author: YongDu
// Date  : 2022-08-07
//===--------------------------------------------------------------------------------------------------------------===//

#include <iostream>
#include <string>
#include <thread>

#include "thread_pool.h"

void func1(int x) {
    std::cout << "[func1] start: thread_id=" << std::this_thread::get_id() << std::endl;
    std::cout << "[func1] x=" << x << std::endl;
    std::this_thread::sleep_for(seconds(5));
}

void func2(int x) {
    std::cout << "[func2] start: thread_id=" << std::this_thread::get_id() << std::endl;
    std::cout << "[func2] x=" << x << std::endl;
    std::this_thread::sleep_for(seconds(3));
}

int main() {
    tp::DThreadPool thPool(4);
    thPool.start();

    thPool.waitForAllDone();
    thPool.stop();

    return 0;
}
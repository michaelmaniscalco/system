#pragma once

#include <thread>
#include <pthread.h>
#include <stdint.h>
#include <chrono>
#include "./cpu_id.h"
#include "./thread/work_contract_group.h"


namespace maniscalco::system
{

    bool set_cpu_affinity
    (
        cpu_id
    );

    cpu_id get_cpu_affinity();

} // namespace maniscalco::system
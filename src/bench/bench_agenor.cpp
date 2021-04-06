// Copyright (c) 2015-2020 The Bitcoin Core developers
// Copyright (c) 2020 The PIVX developers
// Copyright (c) 2021 The AgenorCoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "bench.h"

#include "key.h"
#include "main.h"
#include "util.h"

int
main(int argc, char** argv)
{
    ECC_Start();
    SetupEnvironment();
    g_logger->m_print_to_file = false; // don't want to write to debug.log file

    benchmark::BenchRunner::RunAll();

    ECC_Stop();
}

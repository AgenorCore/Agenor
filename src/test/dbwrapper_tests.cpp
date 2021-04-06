// Copyright (c) 2012-2013 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "dbwrapper.h"
#include "uint256.h"
#include "random.h"
#include "test/test_agenor.h"

#include <boost/assign/std/vector.hpp> // for 'operator+=()'
#include <boost/assert.hpp>
#include <boost/test/unit_test.hpp>

// Test if a string consists entirely of null characters
bool is_null_key(const std::vector<unsigned char>& key) {
    bool isnull = true;

    for (unsigned int i = 0; i < key.size(); i++)
        isnull &= (key[i] == '\x00');

    return isnull;
}

BOOST_FIXTURE_TEST_SUITE(dbwrapper_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(dbwrapper)
{
    {
        fs::path ph = fs::temp_directory_path() / fs::unique_path();
        CDBWrapper dbw(ph, (1 << 20), true, false);
        char key = 'k';
        uint256 in = GetRandHash();
        uint256 res;

        BOOST_CHECK(dbw.Write(key, in));
        BOOST_CHECK(dbw.Read(key, res));
        BOOST_CHECK_EQUAL(res.ToString(), in.ToString());
    }
}

// Test batch operations
BOOST_AUTO_TEST_CASE(dbwrapper_batch)
{
    {
        fs::path ph = fs::temp_directory_path() / fs::unique_path();
        CDBWrapper dbw(ph, (1 << 20), true, false);

        char key = 'i';
        uint256 in = GetRandHash();
        char key2 = 'j';
        uint256 in2 = GetRandHash();
        char key3 = 'k';
        uint256 in3 = GetRandHash();

        uint256 res;
        CDBBatch batch;

        batch.Write(key, in);
        batch.Write(key2, in2);
        batch.Write(key3, in3);

        // Remove key3 before it's even been written
        batch.Erase(key3);

        dbw.WriteBatch(batch);

        BOOST_CHECK(dbw.Read(key, res));
        BOOST_CHECK_EQUAL(res.ToString(), in.ToString());
        BOOST_CHECK(dbw.Read(key2, res));
        BOOST_CHECK_EQUAL(res.ToString(), in2.ToString());

        // key3 never should've been written
        BOOST_CHECK(dbw.Read(key3, res) == false);
    }
}

BOOST_AUTO_TEST_CASE(dbwrapper_iterator)
{
    {
        fs::path ph = fs::temp_directory_path() / fs::unique_path();
        CDBWrapper dbw(ph, (1 << 20), true, false);

        // The two keys are intentionally chosen for ordering
        char key = 'j';
        uint256 in = GetRandHash();
        BOOST_CHECK(dbw.Write(key, in));
        char key2 = 'k';
        uint256 in2 = GetRandHash();
        BOOST_CHECK(dbw.Write(key2, in2));

        boost::scoped_ptr<CDBIterator> it(const_cast<CDBWrapper*>(&dbw)->NewIterator());

        // Be sure to seek past any earlier key (if it exists)
        it->Seek(key);

        char key_res;
        uint256 val_res;

        it->GetKey(key_res);
        it->GetValue(val_res);
        BOOST_CHECK_EQUAL(key_res, key);
        BOOST_CHECK_EQUAL(val_res.ToString(), in.ToString());

        it->Next();

        it->GetKey(key_res);
        it->GetValue(val_res);
        BOOST_CHECK_EQUAL(key_res, key2);
        BOOST_CHECK_EQUAL(val_res.ToString(), in2.ToString());

        it->Next();
        BOOST_CHECK_EQUAL(it->Valid(), false);
    }
}

BOOST_AUTO_TEST_SUITE_END()

/*******************************************************************************
    Copyright (c) 2011 Yahoo! Inc. All rights reserved.

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License. See accompanying LICENSE file.

    The Initial Developer of the Original Code is Shravan Narayanamurthy.
******************************************************************************/
#pragma once

#include "types.hpp"

#include <string>

namespace lda {

static const uint16_t INIT_TC_SIZE = 8; //Initial block allocation for TopicCounts
static const uint16_t SUBSEQ_ALLOCS = 4; //Subsequent allocations for TopicCounts

struct TopicCounts {
    cnt_topic_t* items; //The actual array holding data
    //which is dynamically reshaped
    //This is always sorted in descending
    //order and only has non-zero entries
    //Methods do not check for uniqueness
    //but assume uniqueness.
    //Responsiblity of user to ensure
    //uniqueness

    topic_t length; //The number of elements stored in the array
    topic_t origLength; //The size of the allocated array

    /***** Init *****/
    TopicCounts();
    TopicCounts(int length);
    TopicCounts(cnt_topic_t* it, int len);
    void init(cnt_topic_t* it, int len);
    ~TopicCounts();
    void assign(int length, bool setLen = true);
    void setLength(int length_);
    /***** Init *****/
    /***** Getters *****/
    void findOldnNew(topic_t oldTopic, topic_t newTopic, topic_t** oldTop,
            topic_t** newTop);
    int get_frequency();
    cnt_t get_counts(topic_t topic);
    /***** Getters *****/

    /***** Setters *****/
    bool findAndIncrement(topic_t topic);
    void compact();

    void addNewTop(topic_t topic, cnt_t count = 1);
    void addNewTopAftChk(topic_t topic, cnt_t count = 1);
    void removeOldTop(topic_t ind, cnt_topic_t& ct);

    void decrement(topic_t ind, topic_t** newTop);
    void increment(topic_t ind);

    void upd_count(topic_t old_topic, topic_t new_topic);
    /***** Setters *****/

    // WARNING! this will sort the topics in ascending order. Only use it after
    // the main algorithm finished.
    std::string print();
};

}

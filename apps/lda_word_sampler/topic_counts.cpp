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
#include "topic_counts.hpp"

#include <glog/logging.h>

#include <algorithm>
#include <sstream>

namespace lda {

bool cnt_cmp_ttc(cnt_topic_t i, cnt_topic_t j) {
    return (i.choose.cnt > j.choose.cnt);
}

/********************************** Init *******************************************/

//Constructs an empty TopicCounts structure.
//No memory will be allocated
TopicCounts::TopicCounts() {
    length = 0;
    origLength = 0;
    items = NULL;
}

/**
 * Typical constructor usage. Constructs a TopicCounts
 * structure which can hold length elements. However,
 * this is a dynamic structure and length can change
 * in the process of usage by the use of setter methods
 * This also allocates sufficient storage to store
 * length elements
 */
TopicCounts::TopicCounts(int length) {
    assign(length);
}

/**
 * Constructs TopicCounts from it & len
 */
TopicCounts::TopicCounts(cnt_topic_t* it, int len) {
    assign(len);
    init(it, len);
}

/**
 * This assumes that sufficient memory has already been
 * allocated to hold len elems and just copies them to items
 */
void TopicCounts::init(cnt_topic_t* it, int len) {
    length = len;
    memcpy(items, it, len * sizeof(cnt_topic_t));
}

TopicCounts::~TopicCounts() {
    length = 0;
    origLength = 0;
    if (items != NULL) {
        delete[] items;
        items = NULL;
    }
}

/**
 * The function that allocates memory. origLength
 * memory must be allocated since length can grow
 * upto origLenth. It supports finer granularity
 * access. You can disable the block aligment of
 * lenth which is done by default using setLen
 * if you have already made sure of this
 */
void TopicCounts::assign(int length, bool setLen) {
    if (setLen)
        setLength(length);
    items = new cnt_topic_t[origLength];
}

/**
 * Block aligns the length and sets the
 * origLength parameter to suit that.
 * origLength is always INIT_TC + i*SUBSEQ_ALLOCS
 * where i=0,1,2....
 */
void TopicCounts::setLength(int length_) {
    length = length_;
    if (length < INIT_TC_SIZE) {
        origLength = INIT_TC_SIZE;
    } else {
        length_ -= INIT_TC_SIZE;
        int k = length_ / SUBSEQ_ALLOCS;
        origLength = INIT_TC_SIZE + (k + 1) * SUBSEQ_ALLOCS;
    }
}

/**
 * Typical ways of creating TopicCounts:
 * 1.TopicCounts(length)
 * 2.TopicCounts();assign(length);init(items,length)
 * 3.TopicCounts();assign(-1,false);init(items,length)
 */
/********************************** Init *******************************************/

/********************************* Getters *****************************************/

/**
 * Find oldTopic & newTopic positions in items and retun them in
 * oldTop & newTop. Note that cnt_topic_t is a packed structure.
 * So accessing either the topic or count individually will modify
 * your pointer arithmetic by two
 */
void TopicCounts::findOldnNew(topic_t oldTopic, topic_t newTopic,
        topic_t** oldTop, topic_t** newTop) {
    *oldTop = *newTop = NULL;
    topic_t* endItem = (topic_t*) (items + length);
    topic_t* beg = (topic_t*) items;

    for (topic_t* ind = beg; ind < endItem; ind = ind + 2) {
        if (*ind == oldTopic)
            *oldTop = ind;
        if (*ind == newTopic)
            *newTop = ind;
        if (*oldTop != NULL && *newTop != NULL)
            return;
    }
}

int TopicCounts::get_frequency() {
    int sum = 0;
    for (int i = 0; i < length; i++) {
        sum += items[i].choose.cnt;
    }
    return sum;
}

/********************************* Getters *****************************************/

/********************************* Setters *****************************************/
/**
 * Checks if there is memory wastage and tries to
 * reduce it by compacting memory. It does compaction
 * only when there are more than INIT_TC_SIZE + SUBSEQ_ALLOCS
 * entries and there is at least INIT_TC_ZISE memory
 * being wasted. In such a case, it compact SUBSEQ_ALLOCS
 * of memory. Usually SUBSEQ_ALLOCS should be set
 * to half of INIT_TC_SIZE
 *
 * ***** All pointers to items are invalid once
 * this runs successfully. Use extra caution when
 * you use this or some method which uses this *********
 */
void TopicCounts::compact() {
    if (length > INIT_TC_SIZE + SUBSEQ_ALLOCS && origLength - length
            > INIT_TC_SIZE) {
        //Compact SUBSEQ_ALLOCS of memory
        //LOG(INFO) << "Compacting " << SUBSEQ_ALLOCS
        //        << " bytes of memory from TopicCounts";
        cnt_topic_t* tmp_items = new cnt_topic_t[origLength - SUBSEQ_ALLOCS];
        origLength -= SUBSEQ_ALLOCS;
        memcpy(tmp_items, items, length * sizeof(cnt_topic_t));
        delete[] items;
        items = tmp_items;
    }
}

/**
 * If you want to increment the count of a topic
 * by 1 without knowing the index, you can use
 * this method. But usually increment and decrement
 * are joint operations and you would not want to
 * iterate through the entire array twice. Its
 * advantageous to use the findOldnNew function
 * and increment & decrement methods directly as
 * in TypeTopicCounts::upd_count(word,oldTop,newTop)
 */
bool TopicCounts::findAndIncrement(topic_t topic) {
    topic_t* endItem = (topic_t*) (items + length);
    topic_t* beg = (topic_t*) items;
    for (topic_t* ind = beg; ind < endItem; ind = ind + 2) {
        if (*ind == topic) {
            increment((ind - beg) / 2);
            return true;
        }
    }
    return false;
}

cnt_t TopicCounts::get_counts(topic_t topic) {
    topic_t* endItem = (topic_t*) (items + length);
    topic_t* beg = (topic_t*) items;
    for (topic_t* ind = beg; ind < endItem; ind = ind + 2) {
        if (*ind == topic) {
            return *((cnt_t*) (ind) + 1);
        }
    }
    return 0;
}

/**
 * Adds a new topic to the array making sure to
 * resize it if required. Doesn't check for uniqueness
 * Also assumes that memory allocation has happened
 */
void TopicCounts::addNewTop(topic_t topic, cnt_t count) {
    if (length < origLength) {
        items[length].choose.cnt = count;
        items[length].choose.top = topic;
        ++length;
    } else {
        cnt_topic_t* tmp_items = new cnt_topic_t[length + SUBSEQ_ALLOCS];
        origLength += SUBSEQ_ALLOCS;
        memcpy(tmp_items, items, length * sizeof(cnt_topic_t));
        tmp_items[length].choose.cnt = count;
        tmp_items[length].choose.top = topic;
        ++length;
        delete[] items;
        items = tmp_items;
    }
}

/**
 * Same as above but also checks if memory has been
 * allocated. If not, tries to allocate memory. The
 * check for allocated memory is not very robust as
 * all the members are public and malicious users
 * can set things such that it does not work.
 */
void TopicCounts::addNewTopAftChk(topic_t topic, cnt_t count) {
    if (items == NULL || origLength == 0) {
        length = 1;
        items = new cnt_topic_t[INIT_TC_SIZE];
        origLength = INIT_TC_SIZE;
        items[0].choose.cnt = count;
        items[0].choose.top = topic;
    } else
        addNewTop(topic, count);
}

/**
 * Should be called when you know that ct.choose.cnt==1
 * or ct.choose.cnt-1==0. This will logically remove this
 * entry from the array also taking care of repointing the
 * newTop to the correct position once ct is removed. After
 * length is decremented it is checked if compaction is needed
 * and if so, SUBSEQ_ALLOCS of memory is compacted. The removal
 * is simply a swap with the last element since the current count
 * is 1 and the items array is always sorted in descending order
 * and only has non-zero entries. Also all pointers to items
 * are going to be invalid if compaction runs successfully.
 *
 * ****** Be extra careful while using this *******
 */
void TopicCounts::removeOldTop(topic_t ind, cnt_topic_t& ct) {
    if (length - 1 != 0) {
        cnt_topic_t tmp = ct;
        ct = items[length - 1];
        items[length - 1] = tmp;
    }
    --length;
    compact();
}

/**
 * Used to decrement the count of the topic found at
 * index ind by 1. Also takes care of repointing newTop
 * if its position changes.
 */
void TopicCounts::decrement(topic_t ind, topic_t** newTop) {
    cnt_topic_t& ct = items[ind];
    if (ct.choose.cnt - 1 == 0) {
        //cnt is being zeroed
        //Figure out the index before calling removeOldTop
        //as it might do compaction and make items point
        //somewhere else. Also note that *newTop might be
        //NULL. In this case we need to leave *newTop NULL
        //Hence set it to length to force its set to NULL
        //in the subsequent lines
        int new_top_ind = (*newTop != NULL) ? (*newTop - (topic_t*) (items))
                / 2 : length;

        //If newTop points to the last element,
        //then removal will swap the last element
        //into ind position in items.
        if (new_top_ind == length - 1)
            new_top_ind = ind;

        //Remove and do compaction if necessary
        removeOldTop(ind, ct);

        //Reset *newTop as compact might have changed items
        //utilizing the earlier index calculated before calling
        //remove. Also if it happens that newTop was pointing
        //to the last element and we removed the last element
        //we set it to NULL so that a new topic will be added
        //by TypeTopicCounts
        *newTop = (new_top_ind <= length - 1) ? (topic_t*) &items[new_top_ind]
                : NULL;
        return;
    }

    if (ind < (length - 1) && (ct.choose.cnt - 1) < items[ind + 1].choose.cnt) {
        //decrementing changes the order. So find
        //the element with which this needs to be
        //swapped. Note that the decrement is by 1.
        //Let 10, 9, 8, 8, 8, 6, 6 be the counts
        //To decrement zero based index 2, all we
        //need to do is swap index 4 with 2. As the
        //array is sorted we use binary search to find
        //the position
        cnt_topic_t tmp = ct;
        --tmp.choose.cnt;

        //Find the first element that is not greater than tmp
        //decrement the position and swap with ind
        cnt_topic_t* src = std::lower_bound(items + (ind + 1),
                items + (length), tmp, cnt_cmp_ttc);
        //In the example the above finds the index 5.
        --src; //Note that this also takes care of failure case
        //when lower_bound returns end
        //The above gives index 4

        //Check if we are swapping out topic pointed by newTop
        //If so repoint
        if ((topic_t*) src == *newTop)
            *newTop = (topic_t*) (items + ind);

        //Swap src & index
        items[ind] = *src;
        *src = tmp;
    } else
        //decrementing doesn't change the order
        --ct.choose.cnt; //so simply decrement in place
}

/**
 * Used to increment by 1 the count of an existing topic found at
 * index ind. This assumes that the ind is in range and hence that
 * the topic exists
 */
void TopicCounts::increment(topic_t ind) {
    cnt_topic_t& ct = items[ind];

    if (ind > 0 && (ct.choose.cnt + 1) > items[ind - 1].choose.cnt) {
        //incrementing changes the order. So find
        //the element with which this needs to be
        //swapped. Note that the increment is by 1.
        //Let 10, 9, 8, 8, 8, 6, 6 be the counts
        //To increment zero based index 4, all we
        //need to do is swap index 2 with 4. As the
        //array is sorted we use binary search to find
        //the position
        cnt_topic_t tmp = ct;
        ++tmp.choose.cnt;

        //Find the first element that is lesser than tmp
        //and swap with ind
        cnt_topic_t* src = std::upper_bound(items, items + ind, tmp,
                cnt_cmp_ttc);
        //In the example the above finds the index 2.
        //Note that on failure upper_bound returns end
        //which is items+ind. So its automatically handled

        //Swap src & ind
        items[ind] = *src;
        *src = tmp;
    } else
        //incrementing doesn't change order
        ++ct.choose.cnt; //So just increment in place
}

void TopicCounts::upd_count(topic_t old_topic, topic_t new_topic) {
  if (old_topic == new_topic)
    return;
  topic_t *oldTop = NULL;
  topic_t *newTop = NULL;
  findOldnNew(old_topic, new_topic, &oldTop, &newTop);
  CHECK_NOTNULL(oldTop);
  decrement((oldTop - (topic_t*)items) / 2, &newTop);
  if (newTop == NULL)
    addNewTop(new_topic);
  else
    increment((newTop - (topic_t*)items) / 2);
}

/********************************* Setters *****************************************/

// temp function used below
bool ordered_topics(const cnt_topic_t& lhs, const cnt_topic_t& rhs) {
  return (lhs.choose.top < rhs.choose.top);
}

std::string TopicCounts::print() {
  std::sort(items, items + length, ordered_topics);
  std::stringstream ss;
  for (int i = 0; i < length; i++) {
    ss << " " << items[i].choose.top << ":" << items[i].choose.cnt;
  }
  return ss.str();
}

}

/*
 * Quick start for C++ package for Arduino (ESP boards)
 * 
 * Bojan Jurca, April 18, 2024
 */


#include "Cstring.hpp"
#include "vector.hpp"
#include "queue.hpp"
#include "Map.hpp"
#include "console.hpp"


void setup () {

    // Quick start with console
    cinit (true);                                             // 3 optional arguments: bool waitForSerial = false, unsigned int serialSpeed = 115200, unsigned int waitAfterSerial = 1000

    cout << endl << "--- console ---" << endl << endl;
    cout << "Please enter an integer: ";
    int i;
    cin >> i;
    cout << "you entered " << i << endl;


    // Quick start with Cstring (string is, by default, #defined as Cstring<300>)
    cout << endl << "--- Cstring ---" << endl << endl;

    cstring s;                                                // create a Cstring of max 25 characters on the stack
    cstring t;                                                // create a Cstring with (default) max 300 characters
    s = "thirty tree";                                        // initialize the Cstring
    s = s + ", ...";                                          // calculate with Cstring
    cout << s << endl;

    if (s.errorFlags ())                                      // check if s is in error state
        cout << "Cstring error" << endl;                      // you may want to check individual flags: err_overflow or OUT_OF_RANGE


    // Quick start with vector
    cout << endl << "--- vector ---" << endl << endl;

    vector<int> v;                                            // create a vector containing integers
    v.push_back (10);                                         // add element 10
    v.push_back (20);
    v.push_back (30);
    int f = v.find (20);                                      // find and element
    if (f >= 0)
        cout << "20 found on position " <<  f << endl;
    cout << "Element at position 2 is " << v [2] << endl;     // access an element by its position
    for (auto e: v)                                           // scan all vector elements
        cout << e << endl;
    cout << v << endl;                                        // display all vector elements at once

    if (v.errorFlags ())                                      // check if v is in error state
        cout << "vector error" << endl;                       // you may want to check individual flags: BAD_ALLOC, err_out_of_range or err_not_found 


    // Quick start with queue
    cout << endl << "--- queue ---" << endl << endl;

    queue<int, 10> q;                                         // create a circular queue of integers containing max 10 elements
    for (int i = 0; i < 100; i ++)
        q.push_back (i);                                      // add elements
    for (int i = q.size () - 1; i >= 0; i--)                  // list all queue elements in FIFO order
        cout << q [i] << endl;
    cout << q << endl;                                        // display all queue elements at once

    if (q.errorFlags ())                                      // check if v is in error state
        cout << "queue error" << endl;                        // you may want to check individual flags: err_bad_alloc or OUT_OF_RANGE


    // Quick start with Map
    cout << endl << "--- Map ---" << endl << endl;

    Map<int, cstring> mp;                                     // create a map - key-value pairs, where key is and integer and value is string
    mp [1] = "one";                                           // assign value of "one" to key 1
    mp [2] = "two";
    mp [3] = "tree";
    cout << mp [1] << endl;                                   // access the value if the key is known
    for (auto pair: mp)                                       // scan all key-value pairs in the map
        cout << pair->key << "-" << pair->value << endl;

    if (mp.errorFlags ())                                     // check if mp is in error state
        cout << "Map error" << endl;                          // you may want to check individual flags: err_bad_alloc, err_not_found, err_not_unique or err_cant_do_it_now
}

void loop () {

}

// based off http://hertaville.com/introduction-to-accessing-the-raspberry-pis-gpio-in-c.html

#include <chrono>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <utility>
#include "ir_reader.h"

const int LED_ON = 0; // for my system, LOW -> receiver detected infrared
const int LED_OFF = 1;
const int TIMEOUT_MS = 5000; // 5 seconds of no change seems reasonable

using namespace std;

IrReader::IrReader(int num)
{
    if (num < 0 || num > 31)
    {
        cout << "Bad GPIO val\n";
        exit(1);
    }
    this->gpionum = to_string(num);
    string export_str = "/sys/class/gpio/export";
    ofstream exportgpio(export_str);
    if (!exportgpio){
	cout << "Unable to export GPIO... do you have root? "<< this->gpionum <<"\n";
	exit(1);
    }
    exportgpio << this->gpionum; //write GPIO number to export
    exportgpio.close();
    // we only need to set it as an input in this case
    string setdir_str ="/sys/class/gpio/gpio" + this->gpionum + "/direction";
    ofstream setdirgpio(setdir_str); // open direction file for gpio
    if (!setdirgpio){
	cout << "Unable to set direction of GPIO"<< this->gpionum << "\n";
	exit(1);
    }
    setdirgpio << "in";
    setdirgpio.close();
}

IrReader::~IrReader()
{
    string unexport_str = "/sys/class/gpio/unexport";
    ofstream unexportgpio(unexport_str);
    if (!unexportgpio){
	cout << "Unable to unexport GPIO."<< this->gpionum <<"\n";
	exit(1);
    }
    unexportgpio << this->gpionum ; //write GPIO number to unexport
    unexportgpio.close(); //close unexport file
}

int IrReader::get_val()
{
    static string getval_str = "/sys/class/gpio/gpio" + this->gpionum + "/value";
    ifstream getvalgpio(getval_str);// open value file for gpio
    if (!getvalgpio){
	cout << "Unable to get value of GPIO"<< this->gpionum <<"\n";
	return -1;
    }
    int val;
    getvalgpio >> val ;  //read gpio value
    getvalgpio.close(); //close the value file
    return val;
}

vector<string> IrReader::get_code()
{
    vector<int64_t> codes;
    while (get_val() != LED_ON) // LOW -> IR detected
    {
        // keep spinning until we get a LOW
    }
    // we can assume IR is on right now because we just exitted the loop
    // pair <on/off, time when detected>
    auto stat = make_pair(LED_ON, chrono::high_resolution_clock::now());
    int a = 0;
    while (true)
    {
        if (get_val() != get<0>(stat)) //status changed, we should record
        {
           auto now = chrono::high_resolution_clock::now();
           auto diff = now - get<1>(stat);
           if (get<0>(stat) == LED_ON) // means record positive number
           {
               codes.push_back(chrono::duration_cast<chrono::microseconds>(diff).count());
           }
           else
           {
               codes.push_back(-(chrono::duration_cast<chrono::microseconds>(diff).count()));
           }
           stat = make_pair(1-get<0>(stat), now);
        }
        // check if LED remains off for a while, if so break
        if (a++ % 1000 == 0 && get<0>(stat) == LED_OFF)
        {
            auto now = chrono::high_resolution_clock::now();
            auto diff = now - get<1>(stat);
            if (diff > chrono::milliseconds(TIMEOUT_MS))
            {
                break;
            }
        }
    }
    std::vector<string> stringed_codes;
    for (int i = 0; i < codes.size(); i++)
    {
        stringed_codes.push_back(to_string(codes[i]));
    }
    return stringed_codes;
}

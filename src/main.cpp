/*
 * Smart-Fan-Controller.cpp
 *
 * Created: 02-Sept-17 13:02:26
 * ELECTENG 311 Smart Fan Project
 * Group 4
 */ 

#include <avr/io.h>
#include <string.h>
#include "tinyjsonpp.h"

int main(void)
{
	tinyjsonpp* json = new tinyjsonpp(false, 255);

	char* string = static_cast<char*>(calloc(60, sizeof(char)));
	string = "{\"3\":{\"spd\":{\"req\":\"120\",\"cur\":\"123\"},\"pwr\":{\"req\":\"360\"}}}";

	for (unsigned int i = 0; i < strlen(string); i++) {
		json->parse(string[i]);
	}

	memset(string, 0, strlen(string));
	free(string);

	Value val;
	//val = json->getValue("req");
	val = json->getValue("req", "3/pwr");
	json->insert("hello", "world", "3/pwr");
	val = json->getValue("hello", "3/pwr");
    while (1) 
    {
    }
}


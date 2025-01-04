// data_structures.h
#ifndef DATA_STRUCTURES_H
#define DATA_STRUCTURES_H

typedef enum {
    DATA_SOURCE_SENSOR,
    DATA_SOURCE_CHECKER
} data_source_t;

typedef struct {
    data_source_t source;  // Identify the data source
    float temperature1;
    float temperature2;
    float deviation;  // Deviation calculated by Checker task
    // Add additional fields if necessary
} sensor_data_t;

#endif  // DATA_STRUCTURES_H
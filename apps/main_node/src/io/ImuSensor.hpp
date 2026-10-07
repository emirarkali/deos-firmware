#pragma once

class ImuSensor {
public:
    static ImuSensor& getInstance();
    void init();

private:
    ImuSensor() = default;
};

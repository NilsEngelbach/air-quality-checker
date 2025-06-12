#include <Servo.h>

class BirdyServo
{
public:
    BirdyServo(uint8_t pin);
    void initialize();
    void updatePosition(float iaqValue);

private:
    Servo servo;
    uint8_t pin;
};
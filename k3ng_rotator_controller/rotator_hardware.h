/* rotator_hardware.h

   This fork targets a single board (RemoteQTH Rotator Interface v3.3) and does not use the alternate
   HARDWARE_xxx selected rotator_features_<board>.h / rotator_pins_<board>.h / rotator_settings_<board>.h
   mechanism upstream uses for other builders' boards - those alternate profile files were removed.
   The default rotator_features.h/rotator_pins.h/rotator_settings.h are edited directly instead.

*/

#if !defined(rotator_hardware_h)   // can't touch this
#define rotator_hardware_h         // can't touch this


/* Serial port class definitions for various devices


  For Arduino Leonardo, Micro, and Yún, PLEASE READ THIS ! :

    If using Serial (USB Serial) for the control (main) port, set CONTROL_PORT_SERIAL_PORT_CLASS to Serial_
    If using Serial1 (Board pins 0 and 1 Serial) for the control (main) port, set CONTROL_PORT_SERIAL_PORT_CLASS to HardwareSerial

  For more information on serial ports on various boards: https://www.arduino.cc/reference/en/language/functions/communication/serial/  

*/

#if defined(ARDUINO_MAPLE_MINI)
  #define CONTROL_PORT_SERIAL_PORT_CLASS USBSerial
#elif defined(ARDUINO_AVR_MICRO) || defined(ARDUINO_AVR_LEONARDO) || defined(ARDUINO_AVR_YUN)
  #define CONTROL_PORT_SERIAL_PORT_CLASS Serial_                             // <- Arduino Leonardo, Micro, and Yún - Configure this
#elif defined(ARDUINO_AVR_PROMICRO)  || defined(ARDUINO_AVR_ESPLORA) || defined(ARDUINO_AVR_LILYPAD_USB) || defined(ARDUINO_AVR_ROBOT_CONTROL) || defined(ARDUINO_AVR_ROBOT_MOTOR) || defined(ARDUINO_AVR_LEONARDO_ETH)
  #define CONTROL_PORT_SERIAL_PORT_CLASS Serial_
#elif defined(TEENSYDUINO)
  #define CONTROL_PORT_SERIAL_PORT_CLASS usb_serial_class
#else
  #define CONTROL_PORT_SERIAL_PORT_CLASS HardwareSerial
#endif



// do not modify anything below this line

#if defined(HARDWARE_M0UPU) || defined(HARDWARE_EA4TX_ARS_USB) || defined(HARDWARE_WB6KCN) || defined(HARDWARE_TEST) || defined(HARDWARE_WB6KCN_K3NG)
  #define HARDWARE_CUSTOM
#endif


#endif //!defined(rotator_hardware_h) stop.  hammer time.

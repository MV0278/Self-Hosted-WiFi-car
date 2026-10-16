# SerBot — ESP32 Wi-Fi Robot Car

A Wi-Fi-controlled, two-wheel differential-drive robot car developed as a course project for **21MEO106T – Introduction to Robotics** at **SRM Institute of Science and Technology, Kattankulathur**.

SerBot uses an ESP32 to create its own Wi-Fi hotspot and host a browser-based control interface. Users can control the robot using on-screen buttons on a phone or laptop, or with the **W, A, S, D** keys. An L298N H-bridge motor driver controls the two DC motors.

The system is designed to operate locally without an external Wi-Fi router or internet connection.

## Features

- **Standalone Wi-Fi:** ESP32 creates its own access point (SoftAP).
- **Browser-based interface:** Control the robot without installing a dedicated application.
- **Multiple control methods:** On-screen buttons, touch controls, and W/A/S/D keyboard input.
- **Differential-drive motion:** Forward, backward, left, right, and stop commands.
- **Performance measurement:** Instrumented firmware for latency, Wi-Fi RSSI, error calculations, and CSV export.
- **Experimental evaluation:** Test communication latency at different distances and measure robot movement performance.

## Hardware Requirements

- ESP32 development board
- L298N dual H-bridge motor driver
- 2 DC geared motors with wheels
- Robot chassis
- Battery pack and power switch
- Connecting wires
- Phone or laptop with a Wi-Fi-enabled browser

## Hardware Pin Configuration

| L298N Pin | ESP32 GPIO | Function |
|---|---:|---|
| IN1 | 26 | Left motor forward |
| IN2 | 27 | Left motor backward |
| IN3 | 14 | Right motor forward |
| IN4 | 12 | Right motor backward |
| ENA | 25 | Left motor enable |
| ENB | 15 | Right motor enable |

**Note:** GPIO 12 is a boot-strapping pin on the ESP32. If the board fails to boot when the motor driver is connected, move IN4 to GPIO 13 or GPIO 33 and update the firmware accordingly.

## Repository Structure

```text
SerBot/
├── firmware/
│   ├── Robot_Car.cpp
│   └── SerBot_DataLogger.ino
├── data/
│   └── CSV exports from test runs
├── docs/
│   ├── circuit diagram
│   └── review slides
├── literature/
│   └── notes on reviewed papers
└── README.md
```

- `Robot_Car.cpp` — Original robot control firmware.
- `SerBot_DataLogger.ino` — Instrumented firmware for performance measurements and CSV export.
- `data/` — Experimental data collected during testing.
- `docs/` — Circuit diagrams and project review presentations.
- `literature/` — Literature survey notes and research references.

## Getting Started

### 1. Upload the Firmware

1. Open Arduino IDE.
2. Select **ESP32 Dev Module** as the target board.
3. Open `firmware/SerBot_DataLogger.ino`.
4. Install the required ESP32 board package and libraries if they are not already available.
5. Compile and upload the firmware to the ESP32.

### 2. Connect to the Robot

1. Power on the robot.
2. Open the Wi-Fi settings on your phone or laptop.
3. Connect to the following network:

   | Setting | Value |
   |---|---|
   | Wi-Fi SSID | `RoboCar` |
   | Password | `robot123` |
   | Control page | `http://192.168.4.1` |

4. Open the control page in your browser.

### 3. Control the Robot

| Input | Action |
|---|---|
| W | Move forward |
| S | Move backward |
| A | Turn left |
| D | Turn right |
| STOP | Stop the motors |

**Safety:** Test the robot in an open area, keep the wheels clear of obstructions during initial testing, and be prepared to disconnect power if the robot behaves unexpectedly.

## Performance Testing

The instrumented firmware is intended to support the following experiments.

### 1. Wi-Fi Command Latency

Measure the response time for 50 safe STOP-command requests. Set the test distance and repeat at:

- 1 m
- 5 m
- 10 m
- 15 m
- 20 m

Record the latency measurements and Wi-Fi signal strength (RSSI) for each test distance.

### 2. Linear Speed

1. Mark a straight test path of 2 metres.
2. Run the robot over the path five times.
3. Record the time taken for each run.
4. Calculate the average time and estimated linear speed.

### 3. Turning Performance

1. Command the robot to perform a 90-degree turn.
2. Record the time taken for the turn.
3. Repeat the test five times.
4. Compare measured turning time with the theoretical estimate.

### 4. CSV Data Export

Use the data-logging interface to download the experimental results as a CSV file. Store the exported files in the `data/` directory for analysis and documentation.

## Mathematical Model

### Wheel Linear Speed

The theoretical linear speed of a wheel is:

\[
v = \frac{\pi D N}{60}
\]

Where:

- \(v\) = linear wheel speed (m/s)
- \(D\) = wheel diameter (m)
- \(N\) = rotational speed (RPM)

### Differential-Drive Kinematics

The robot's linear and angular velocities are:

\[
V = \frac{v_R + v_L}{2}
\]

\[
\omega = \frac{v_R - v_L}{b}
\]

Where:

- \(V\) = robot linear velocity (m/s)
- \(\omega\) = robot angular velocity (rad/s)
- \(v_R\) = right-wheel linear velocity (m/s)
- \(v_L\) = left-wheel linear velocity (m/s)
- \(b\) = distance between the two wheels (m)

### Theoretical 90-Degree Pivot Turn

For a turn where one wheel is stopped and the other moves at speed \(v\):

\[
t_{90} = \frac{\pi b}{2v}
\]

Where:

- \(t_{90}\) = theoretical time for a 90-degree turn (s)
- \(b\) = distance between the wheels (m)
- \(v\) = linear speed of the moving wheel (m/s)

This estimate assumes ideal motion and should be compared with measured turning time.

### Error Metrics

**Mean Absolute Error (MAE)**

\[
MAE = \frac{1}{n}\sum_{i=1}^{n}|e_i|
\]

**Root Mean Square Error (RMSE)**

\[
RMSE = \sqrt{\frac{1}{n}\sum_{i=1}^{n}e_i^2}
\]

**Percentage Error**

\[
\text{Percentage Error} =
\frac{|x_{\text{measured}}-x_{\text{theoretical}}|}
{|x_{\text{theoretical}}|}\times100
\]

Where \(e_i\) represents an individual measurement error, and \(n\) is the number of measurements.

## Current Development Status

- [x] Motor control and SoftAP web interface
- [x] W/A/S/D keyboard controls
- [x] Touch and on-screen button controls
- [x] Data-logging firmware with performance-metric support
- [ ] Complete experimental tests and commit the collected data
- [ ] Implement PWM-based motor speed control
- [ ] Implement a heartbeat watchdog for automatic stopping when the control connection is lost

## Future Enhancements

- PWM-based speed regulation
- Automatic stop on communication loss
- Improved turning accuracy
- Extended latency and reliability experiments
- Data visualization and comparative performance analysis

## Project Objective

The objective of SerBot is to develop and evaluate a low-cost, self-hosted robotic car that can be controlled through a standard web browser without requiring an external router, internet connection, or dedicated mobile application.

The project also aims to quantify communication and motion performance through repeatable experiments and documented data.

## Course Information

- **Course:** 21MEO106T – Introduction to Robotics
- **Institution:** SRM Institute of Science and Technology
- **Campus:** Kattankulathur
- **Project:** SerBot — ESP32 Wi-Fi Robot Car

---

*Developed as an academic robotics project. Experimental results will be added after testing is completed.*

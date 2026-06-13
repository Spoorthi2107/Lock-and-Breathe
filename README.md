# Lock 'n' Breathe – Alcohol Detection and Engine Locking System

## Overview

Lock 'n' Breathe is an embedded vehicle safety system designed to prevent drunk driving by detecting alcohol concentration in a driver's breath before vehicle ignition.

## Objective

To develop a smart vehicle safety system that prevents vehicle ignition when alcohol is detected in the driver's breath, thereby reducing drunk-driving-related accidents.

## Problem Statement

Drunk driving is one of the major causes of road accidents. Traditional enforcement methods rely on manual intervention and cannot provide continuous monitoring. This project aims to provide an automated and preventive solution.

## Features

* Alcohol detection using MQ-3 sensor
* Automatic engine locking mechanism
* Multi-level alcohol classification
* GPS-based location tracking
* LCD status display
* Buzzer alerts
* SD card event logging
* ThingSpeak IoT integration
* Keypad-based emergency override

## Hardware Components

* Arduino Uno R4 Wi-Fi
* MQ-3 Alcohol Sensor
* GPS Module (NEO-6M)
* Relay Module
* LCD Display
* Buzzer
* SD Card Module
* Keypad

## Software Used

* Arduino IDE
* ThingSpeak IoT Platform

## Working

The system detects alcohol concentration from the driver's breath and classifies it into safe, low, medium, or high levels. Depending on the detected level, the system either allows ignition, requests password-based authorization, or locks the engine while generating alerts and logging events.

## Project Structure

* Source Code (.ino)
* Project Report
* Documentation

## Team Members

* AV Spoorthi
* Ananya Jaiswal
* Samiran Saha
* Akshat Agrawal
* Niharika Yarramsetty
* Saraswathi Thanusha

## Results

- Successfully detected alcohol levels using MQ-3 sensor.
- Engine ignition disabled when alcohol exceeded threshold limits.
- Real-time monitoring through ThingSpeak dashboard.

## Future Scope

* Mobile application integration
* Face recognition-based authentication
* Enhanced sensor accuracy
* Vehicle speed monitoring under emergency override

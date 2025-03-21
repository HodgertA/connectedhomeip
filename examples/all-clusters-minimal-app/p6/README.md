#CHIP P6 All Clusters Example

An example showing the use of Matter on the Infineon CY8CKIT-062S2-43012 board.

<hr>

-   [Matter P6 All Clusters Example](#chip-p6-clusters-example)
    -   [Introduction](#introduction)
    -   [Building](#building)
    -   [Flashing the Application](#flashing-the-application)
    -   [Commissioning and cluster control](#commissioning-and-cluster-control)
        -   [Setting up chip-tool](#setting-up-chip-tool)
        -   [Commissioning over BLE](#commissioning-over-ble)
            -   [Notes](#notes)

<hr>

<a name="intro"></a>

## Introduction

The P6 clusters example provides a baseline demonstration of a Cluster control
device, built using Matter and the Infineon Modustoolbox SDK. It can be
controlled by Matter controller over Wi-Fi network.

The P6 device can be commissioned over Bluetooth Low Energy where the device and
the Matter controller will exchange security information with the Rendezvous
procedure. Wi-Fi Network credentials are then provided to the P6 device which
will then join the network.

<a name="building"></a>

## Building

-   [Modustoolbox Software](https://www.cypress.com/products/modustoolbox)

    Refer to `integrations/docker/images/chip-build-infineon/Dockerfile` or
    `scripts/examples/gn_p6_example.sh` for downloading the Software and related
    tools.

-   Install some additional tools (likely already present for Matter
    developers):

    ```
    sudo apt install gcc g++ clang ninja-build python \
      python3-venv libssl-dev libavahi-client-dev libglib2.0-dev git cmake \
      python3-pip
    ```

-   Supported hardware:
    [CY8CKIT-062S2-43012](https://www.cypress.com/CY8CKIT-062S2-43012)

*   Build the example application:

          ```
          $ ./scripts/examples/gn_p6_example.sh ./examples/all-clusters-minimal-app/p6 out/minimal_clusters_app_p6
          ```

-   To delete generated executable, libraries and object files use:

          ```
          $ cd ~/connectedhomeip
          $ rm -rf out/
          ```

<a name="flashing"></a>

## Flashing the Application

-   Put CY8CKIT-062S2-43012 board on KitProg3 CMSIS-DAP Mode by pressing the
    `MODE SELECT` button. `KITPROG3 STATUS` LED is ON confirms board is in
    proper mode.

-   On the command line:

          ```
          $ cd ~/connectedhomeip
          $ python3 out/clusters_app_p6/chip-p6-clusters-minimal-example.flash.py
          ```

<a name="Commissioning and cluster control"></a>

## Commissioning and cluster control

Commissioning can be carried out using BLE.

<a name="Setting up chip-tool"></a>

### Setting up Chip tool

Once P6 is up and running, we need to set up chip-tool on Raspberry Pi 4 to
perform commissioning and cluster control.

-   Set up python controller.

           ```
           $ cd {path-to-connectedhomeip}
           $ ./scripts/examples/gn_build_example.sh examples/chip-tool out/debug
           ```

-   Execute the controller.

           ```
           $ ./out/debug/chip-tool
           ```

<a name="Commissioning over BLE"></a>

### Commissioning over BLE

Run the built executable and pass it the discriminator and pairing code of the
remote device, as well as the network credentials to use.

         ```
         $ ./out/debug/chip-tool pairing ble-wifi 1234 ${SSID} ${PASSWORD} 20202021 3840

         Parameters:
         1. Discriminator: 3840
         2. Setup-pin-code: 20202021
         3. Node ID: 1234 (you can assign any node id)
         4. SSID : Wi-Fi SSID
         5. PASSWORD : Wi-Fi Password
         ```

<a name="Notes"></a>

#### Notes

Raspberry Pi 4 BLE connection issues can be avoided by running the following
commands. These power cycle the BlueTooth hardware and disable BR/EDR mode.

          ```
          sudo btmgmt -i hci0 power off
          sudo btmgmt -i hci0 bredr off
          sudo btmgmt -i hci0 power on
          ```

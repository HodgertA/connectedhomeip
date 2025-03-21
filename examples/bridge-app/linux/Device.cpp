/*
 *
 *    Copyright (c) 2021 Project CHIP Authors
 *    Copyright (c) 2019 Google LLC.
 *    All rights reserved.
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#include "Device.h"

#include <cstdio>
#include <platform/CHIPDeviceLayer.h>

// LightingManager LightingManager::sLight;

Device::Device(const char * szDeviceName, const char * szLocation)
{
    strncpy(mName, szDeviceName, sizeof(mName));
    strncpy(mLocation, szLocation, sizeof(mLocation));
    mReachable  = false;
    mEndpointId = 0;
}

bool Device::IsReachable()
{
    return mReachable;
}

void Device::SetReachable(bool aReachable)
{
    bool changed = (mReachable != aReachable);

    mReachable = aReachable;

    if (aReachable)
    {
        ChipLogProgress(DeviceLayer, "Device[%s]: ONLINE", mName);
    }
    else
    {
        ChipLogProgress(DeviceLayer, "Device[%s]: OFFLINE", mName);
    }

    if (changed)
    {
        HandleDeviceChange(this, kChanged_Reachable);
    }
}

void Device::SetName(const char * szName)
{
    bool changed = (strncmp(mName, szName, sizeof(mName)) != 0);

    ChipLogProgress(DeviceLayer, "Device[%s]: New Name=\"%s\"", mName, szName);

    strncpy(mName, szName, sizeof(mName));

    if (changed)
    {
        HandleDeviceChange(this, kChanged_Name);
    }
}

void Device::SetLocation(const char * szLocation)
{
    bool changed = (strncmp(mLocation, szLocation, sizeof(mLocation)) != 0);

    strncpy(mLocation, szLocation, sizeof(mLocation));

    ChipLogProgress(DeviceLayer, "Device[%s]: Location=\"%s\"", mName, mLocation);

    if (changed)
    {
        HandleDeviceChange(this, kChanged_Location);
    }
}

DeviceOnOff::DeviceOnOff(const char * szDeviceName, const char * szLocation) : Device(szDeviceName, szLocation)
{
    mOn = false;
}

bool DeviceOnOff::IsOn()
{
    return mOn;
}

void DeviceOnOff::SetOnOff(bool aOn)
{
    bool changed;

    changed = aOn ^ mOn;
    mOn     = aOn;
    ChipLogProgress(DeviceLayer, "Device[%s]: %s", mName, aOn ? "ON" : "OFF");

    if ((changed) && (mChanged_CB))
    {
        mChanged_CB(this, kChanged_OnOff);
    }
}

void DeviceOnOff::SetChangeCallback(DeviceCallback_fn aChanged_CB)
{
    mChanged_CB = aChanged_CB;
}

void DeviceOnOff::HandleDeviceChange(Device * device, Device::Changed_t changeMask)
{
    if (mChanged_CB)
    {
        mChanged_CB(this, (DeviceOnOff::Changed_t) changeMask);
    }
}

DeviceSwitch::DeviceSwitch(const char * szDeviceName, const char * szLocation, uint32_t aFeatureMap) :
    Device(szDeviceName, szLocation)
{
    mNumberOfPositions = 2;
    mCurrentPosition   = 0;
    mMultiPressMax     = 2;
    mFeatureMap        = aFeatureMap;
}

void DeviceSwitch::SetNumberOfPositions(uint8_t aNumberOfPositions)
{
    bool changed;

    changed            = aNumberOfPositions != mNumberOfPositions;
    mNumberOfPositions = aNumberOfPositions;

    if ((changed) && (mChanged_CB))
    {
        mChanged_CB(this, kChanged_NumberOfPositions);
    }
}

void DeviceSwitch::SetCurrentPosition(uint8_t aCurrentPosition)
{
    bool changed;

    changed          = aCurrentPosition != mCurrentPosition;
    mCurrentPosition = aCurrentPosition;

    if ((changed) && (mChanged_CB))
    {
        mChanged_CB(this, kChanged_CurrentPosition);
    }
}

void DeviceSwitch::SetMultiPressMax(uint8_t aMultiPressMax)
{
    bool changed;

    changed        = aMultiPressMax != mMultiPressMax;
    mMultiPressMax = aMultiPressMax;

    if ((changed) && (mChanged_CB))
    {
        mChanged_CB(this, kChanged_MultiPressMax);
    }
}

void DeviceSwitch::SetChangeCallback(DeviceCallback_fn aChanged_CB)
{
    mChanged_CB = aChanged_CB;
}

void DeviceSwitch::HandleDeviceChange(Device * device, Device::Changed_t changeMask)
{
    if (mChanged_CB)
    {
        mChanged_CB(this, (DeviceSwitch::Changed_t) changeMask);
    }
}

void ComposedDevice::HandleDeviceChange(Device * device, Device::Changed_t changeMask)
{
    if (mChanged_CB)
    {
        mChanged_CB(this, (ComposedDevice::Changed_t) changeMask);
    }
}

void DevicePowerSource::HandleDeviceChange(Device * device, Device::Changed_t changeMask)
{
    if (mChanged_CB)
    {
        mChanged_CB(this, (DevicePowerSource::Changed_t) changeMask);
    }
}

void DevicePowerSource::SetBatChargeLevel(uint8_t aBatChargeLevel)
{
    bool changed;

    changed         = aBatChargeLevel != mBatChargeLevel;
    mBatChargeLevel = aBatChargeLevel;

    if ((changed) && (mChanged_CB))
    {
        mChanged_CB(this, kChanged_BatLevel);
    }
}

void DevicePowerSource::SetDescription(std::string aDescription)
{
    bool changed;

    changed      = aDescription != mDescription;
    mDescription = aDescription;

    if ((changed) && (mChanged_CB))
    {
        mChanged_CB(this, kChanged_Description);
    }
}

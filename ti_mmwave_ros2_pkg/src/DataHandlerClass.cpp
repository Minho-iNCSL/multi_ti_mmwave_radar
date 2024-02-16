/*
 * @file DataHandlerClass.cpp
 *
 * @brief
 * Handles and publishes incoming data from the sensor and .
 *
 * \par
 * NOTE:
 * (C) Copyright 2020 Texas Instruments, Inc.
 * ROS 2 Copyright 2022 Swimming Kim, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the
 * distribution.
 *
 * Neither the name of Texas Instruments Incorporated nor the names of
 * its contributors may be used to endorse or promote products derived
 * from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "ti_mmwave_ros2_pkg/DataHandlerClass.h"
#include <stdio.h>

DataUARTHandler::DataUARTHandler()
    : rclcpp::Node("DataUARTHandler"), currentBufp(&pingPongBuffers[0]),
      nextBufp(&pingPongBuffers[1]) {
  //   DataUARTHandler_pub = create_publisher<sensor_msgs::msg::PointCloud2>(
  //       "/ti_mmwave/radar_scan_pcl", 100);
  //   radar_scan_pub =
  //   create_publisher<ti_mmwave_ros2_interfaces::msg::RadarScan>(
  //       "/ti_mmwave/radar_scan", 100);
  //   marker_pub = create_publisher<visualization_msgs::msg::Marker>(
  //       "/ti_mmwave/radar_scan_markers", 100);
  // onInit();
}

void DataUARTHandler::onInit() {

  parameters_client = std::make_shared<rclcpp::AsyncParametersClient>(
      this, ns + "/mmWaveCommSrvNode");

  while (!parameters_client->wait_for_service(std::chrono::seconds(1))) {
    if (!rclcpp::ok()) {
      RCLCPP_ERROR(this->get_logger(),
                   "client interrupted while waiting for service to appear.");
      return;
    }
    RCLCPP_INFO(this->get_logger(), "waiting for service to appear...");
  }

  auto parameters_future = parameters_client->get_parameters(
      {"numAdcSamples", "numLoops", "num_TX", "f_s", "f_c", "BW", "PRI", "t_fr",
       "max_range", "range_resolution", "max_doppler_vel",
       "doppler_vel_resolution"},
      std::bind(&DataUARTHandler::callbackGlobalParam, this,
                std::placeholders::_1));

  maxAllowedElevationAngleDeg = 90; // Use max angle if none specified
  maxAllowedAzimuthAngleDeg = 90;   // Use max angle if none specified
}

void DataUARTHandler::setPublishers(
    const rclcpp::Publisher<PointCloud2>::SharedPtr DataUARTHandler_pub_in,
    const rclcpp::Publisher<RadarScan>::SharedPtr radar_scan_pub_in,
    const rclcpp::Publisher<Marker>::SharedPtr marker_pub_in) {
  this->DataUARTHandler_pub = DataUARTHandler_pub_in;
  this->radar_scan_pub = radar_scan_pub_in;
  this->marker_pub = marker_pub_in;
}

void DataUARTHandler::setNamespace(const std::string &ns) { this->ns = ns; }

void DataUARTHandler::callbackGlobalParam(
    std::shared_future<std::vector<rclcpp::Parameter>> future) {

  auto result = future.get();
  nr = result.at(0).as_int();
  nd = result.at(1).as_int();
  ntx = result.at(2).as_int();

  fs = static_cast<float>(result.at(3).as_double());
  fc = static_cast<float>(result.at(4).as_double());
  BW = static_cast<float>(result.at(5).as_double());
  PRI = static_cast<float>(result.at(6).as_double());
  tfr = static_cast<float>(result.at(7).as_double());
  max_range = static_cast<float>(result.at(8).as_double());
  vrange = static_cast<float>(result.at(9).as_double());
  max_vel = static_cast<float>(result.at(10).as_double());
  vvel = static_cast<float>(result.at(11).as_double());

  printf(
      "\n\n==============================\nList of "
      "parameters\n==============================\nNumber of range samples: "
      "%d\nNumber of chirps: %d\nf_s: %.3f MHz\nf_c: %.3f GHz\nBandwidth: %.3f "
      "MHz\nPRI: %.3f us\nFrame time: %.3f ms\nMax range: %.3f m\nRange "
      "resolution: %.3f m\nMax Doppler: +-%.3f m/s\nDoppler resolution: %.3f "
      "m/s\n==============================\n",
      nr, nd, fs / 1e6, fc / 1e9, BW / 1e6, PRI * 1e6, tfr * 1e3, max_range,
      vrange, max_vel / 2, vvel);
}

void DataUARTHandler::setFrameID(char *myFrameID) { frameID = myFrameID; }

/*Implementation of setUARTPort*/
void DataUARTHandler::setUARTPort(char *mySerialPort) {
  dataSerialPort = mySerialPort;
}

/*Implementation of setBaudRate*/
void DataUARTHandler::setBaudRate(int myBaudRate) { dataBaudRate = myBaudRate; }

/*Implementation of setMaxAllowedElevationAngleDeg*/
void DataUARTHandler::setMaxAllowedElevationAngleDeg(
    int myMaxAllowedElevationAngleDeg) {
  maxAllowedElevationAngleDeg = myMaxAllowedElevationAngleDeg;
}

/*Implementation of setMaxAllowedAzimuthAngleDeg*/
void DataUARTHandler::setMaxAllowedAzimuthAngleDeg(
    int myMaxAllowedAzimuthAngleDeg) {
  maxAllowedAzimuthAngleDeg = myMaxAllowedAzimuthAngleDeg;
}

/*Implementation of readIncomingData*/
void *DataUARTHandler::readIncomingData(void) {

  int firstPacketReady = 0;
  uint8_t last8Bytes[8] = {0};

  /*Open UART Port and error checking*/
  serial::Serial mySerialObject("", dataBaudRate,
                                serial::Timeout::simpleTimeout(100));
  mySerialObject.setPort(dataSerialPort);
  try {
    mySerialObject.open();
  } catch (std::exception &e1) {
    printf("DataUARTHandler Read Thread: Failed to open Data serial port "
           "with error: %s",
           e1.what());
    printf("DataUARTHandler Read Thread: Waiting 20 "
           "seconds before trying again...");
    try {
      // Wait 20 seconds and try to open serial port again
      // ros::Duration(20).sleep();
      rclcpp::sleep_for(std::chrono::seconds(20));
      mySerialObject.open();
    } catch (std::exception &e2) {
      //   TODO
      //   RCLCPP_INFO(this->get_logger(),
      //               "DataUARTHandler Read Thread: Failed second time to open
      //               " "Data serial port, error: %s", e1.what());
      //   RCLCPP_INFO(this->get_logger(),
      //               "DataUARTHandler Read Thread: Port could not be opened.
      //               Port " "is \"%s\" and baud rate is %d", dataSerialPort,
      //               dataBaudRate);

      pthread_exit(NULL);
    }
  }

  if (mySerialObject.isOpen())
    printf("DataUARTHandler Read Thread: Port is open");
  else
    printf("DataUARTHandler Read Thread: Port could not be opened");

  /*Quick magicWord check to synchronize program with data Stream*/
  while (!isMagicWord(last8Bytes)) {

    last8Bytes[0] = last8Bytes[1];
    last8Bytes[1] = last8Bytes[2];
    last8Bytes[2] = last8Bytes[3];
    last8Bytes[3] = last8Bytes[4];
    last8Bytes[4] = last8Bytes[5];
    last8Bytes[5] = last8Bytes[6];
    last8Bytes[6] = last8Bytes[7];
    mySerialObject.read(&last8Bytes[7], 1);
  }

  /*Lock nextBufp before entering main loop*/
  pthread_mutex_lock(&nextBufp_mutex);

  while (rclcpp::ok()) {

    // std::cout << "readIncomingData" << std::endl;

    /*Start reading UART data and writing to buffer while also checking for
     * magicWord*/
    last8Bytes[0] = last8Bytes[1];
    last8Bytes[1] = last8Bytes[2];
    last8Bytes[2] = last8Bytes[3];
    last8Bytes[3] = last8Bytes[4];
    last8Bytes[4] = last8Bytes[5];
    last8Bytes[5] = last8Bytes[6];
    last8Bytes[6] = last8Bytes[7];
    mySerialObject.read(&last8Bytes[7], 1);

    nextBufp->push_back(last8Bytes[7]); // push byte onto buffer

    // ROS_INFO("DataUARTHandler Read Thread: last8bytes = %02x%02x %02x%02x
    // %02x%02x %02x%02x",  last8Bytes[7], last8Bytes[6], last8Bytes[5],
    // last8Bytes[4], last8Bytes[3], last8Bytes[2], last8Bytes[1],
    // last8Bytes[0]);

    /*If a magicWord is found wait for sorting to finish and switch buffers*/
    if (isMagicWord(last8Bytes)) {
      // ROS_INFO("Found magic word");

      /*Lock countSync Mutex while unlocking nextBufp so that the swap thread
       * can use it*/
      pthread_mutex_lock(&countSync_mutex);
      pthread_mutex_unlock(&nextBufp_mutex);

      /*increment countSync*/
      countSync++;

      /*If this is the first packet to be found, increment countSync again since
       * Sort thread is not reading data yet*/
      if (firstPacketReady == 0) {
        countSync++;
        firstPacketReady = 1;
      }

      /*Signal Swap Thread to run if countSync has reached its max value*/
      if (countSync == COUNT_SYNC_MAX) {
        pthread_cond_signal(&countSync_max_cv);
      }

      /*Wait for the Swap thread to finish swapping pointers and signal us to
       * continue*/
      pthread_cond_wait(&read_go_cv, &countSync_mutex);

      /*Unlock countSync so that Swap Thread can use it*/
      pthread_mutex_unlock(&countSync_mutex);
      pthread_mutex_lock(&nextBufp_mutex);

      nextBufp->clear();
      memset(last8Bytes, 0, sizeof(last8Bytes));
    }
  }

  mySerialObject.close();

  pthread_exit(NULL);
}

int DataUARTHandler::isMagicWord(uint8_t last8Bytes[8]) {
  int val = 0, i = 0, j = 0;

  for (i = 0; i < 8; i++) {

    if (last8Bytes[i] == magicWord[i]) {
      j++;
    }
  }

  if (j == 8) {
    val = 1;
  }

  return val;
}

void *DataUARTHandler::syncedBufferSwap(void) {
  while (rclcpp::ok()) {

    // std::cout << "syncedBufferSwap" << std::endl;

    pthread_mutex_lock(&countSync_mutex);

    while (countSync < COUNT_SYNC_MAX) {
      pthread_cond_wait(&countSync_max_cv, &countSync_mutex);

      pthread_mutex_lock(&currentBufp_mutex);
      pthread_mutex_lock(&nextBufp_mutex);

      std::vector<uint8_t> *tempBufp = currentBufp;

      this->currentBufp = this->nextBufp;

      this->nextBufp = tempBufp;

      pthread_mutex_unlock(&currentBufp_mutex);
      pthread_mutex_unlock(&nextBufp_mutex);

      countSync = 0;

      pthread_cond_signal(&sort_go_cv);
      pthread_cond_signal(&read_go_cv);
    }

    pthread_mutex_unlock(&countSync_mutex);
  }

  pthread_exit(NULL);
}

void *DataUARTHandler::sortIncomingData(void) {
  MmwDemo_Output_TLV_Types tlvType = MMWDEMO_OUTPUT_MSG_NULL;
  uint32_t tlvLen = 0;
  uint32_t headerSize;
  unsigned int currentDatap = 0;
  SorterState sorterState = READ_HEADER;
  uint i = 0, tlvCount = 0; //, offset = 0;
  int j = 0;
  float maxElevationAngleRatioSquared;
  float maxAzimuthAngleRatio;

  boost::shared_ptr<pcl::PointCloud<pcl::PointXYZI>> RScan(
      new pcl::PointCloud<pcl::PointXYZI>);
  sensor_msgs::msg::PointCloud2 output_pointcloud;
  ti_mmwave_ros2_interfaces::msg::RadarScan radarscan;

  // wait for first packet to arrive
  pthread_mutex_lock(&countSync_mutex);
  pthread_cond_wait(&sort_go_cv, &countSync_mutex);
  pthread_mutex_unlock(&countSync_mutex);

  pthread_mutex_lock(&currentBufp_mutex);

  while (rclcpp::ok()) {

    // std::cout << "sortIncomingData" << std::endl;

    switch (sorterState) {

    case READ_HEADER:

      // init variables
      mmwData.numObjOut = 0;

      // make sure packet has at least first three fields (12 bytes) before we
      // read them (does not include magicWord since it was already removed)
      if (currentBufp->size() < 12) {
        sorterState = SWAP_BUFFERS;
        break;
      }

      // get version (4 bytes)
      memcpy(&mmwData.header.version, &currentBufp->at(currentDatap),
             sizeof(mmwData.header.version));
      currentDatap += (sizeof(mmwData.header.version));

      // get totalPacketLen (4 bytes)
      memcpy(&mmwData.header.totalPacketLen, &currentBufp->at(currentDatap),
             sizeof(mmwData.header.totalPacketLen));
      currentDatap += (sizeof(mmwData.header.totalPacketLen));

      // get platform (4 bytes)
      memcpy(&mmwData.header.platform, &currentBufp->at(currentDatap),
             sizeof(mmwData.header.platform));
      currentDatap += (sizeof(mmwData.header.platform));

      // if packet doesn't have correct header size (which is based on
      // platform), throw it away
      //  (does not include magicWord since it was already removed)
      if ((mmwData.header.platform & 0xFFFF) == 0x1443) // platform is xWR1443)
      {
        headerSize =
            7 * 4; // xWR1443 SDK demo header does not have subFrameNumber field
      } else {
        headerSize = 8 * 4; // header includes subFrameNumber field
      }
      if (currentBufp->size() < headerSize) {
        sorterState = SWAP_BUFFERS;
        break;
      }

      // get frameNumber (4 bytes)
      memcpy(&mmwData.header.frameNumber, &currentBufp->at(currentDatap),
             sizeof(mmwData.header.frameNumber));
      currentDatap += (sizeof(mmwData.header.frameNumber));

      // get timeCpuCycles (4 bytes)
      memcpy(&mmwData.header.timeCpuCycles, &currentBufp->at(currentDatap),
             sizeof(mmwData.header.timeCpuCycles));
      currentDatap += (sizeof(mmwData.header.timeCpuCycles));

      // get numDetectedObj (4 bytes)
      memcpy(&mmwData.header.numDetectedObj, &currentBufp->at(currentDatap),
             sizeof(mmwData.header.numDetectedObj));
      currentDatap += (sizeof(mmwData.header.numDetectedObj));

      // get numTLVs (4 bytes)
      memcpy(&mmwData.header.numTLVs, &currentBufp->at(currentDatap),
             sizeof(mmwData.header.numTLVs));
      currentDatap += (sizeof(mmwData.header.numTLVs));

      // get subFrameNumber (4 bytes) (not used for XWR1443)
      if ((mmwData.header.platform & 0xFFFF) != 0x1443) {
        memcpy(&mmwData.header.subFrameNumber, &currentBufp->at(currentDatap),
               sizeof(mmwData.header.subFrameNumber));
        currentDatap += (sizeof(mmwData.header.subFrameNumber));
      }

      // if packet lengths do not match, throw it away
      if (mmwData.header.totalPacketLen == currentBufp->size()) {
        sorterState = CHECK_TLV_TYPE;
      } else
        sorterState = SWAP_BUFFERS;

      break;

    case READ_OBJ_STRUCT:

      // CHECK_TLV_TYPE code has already read tlvType and tlvLen

      i = 0;
      // offset = 0;

      if (((mmwData.header.version >> 24) & 0xFF) <
          3) // SDK version is older than 3.x
      {
        // get number of objects
        memcpy(&mmwData.numObjOut, &currentBufp->at(currentDatap),
               sizeof(mmwData.numObjOut));
        currentDatap += (sizeof(mmwData.numObjOut));

        // get xyzQFormat
        memcpy(&mmwData.xyzQFormat, &currentBufp->at(currentDatap),
               sizeof(mmwData.xyzQFormat));
        currentDatap += (sizeof(mmwData.xyzQFormat));
      } else // SDK version is at least 3.x
      {
        mmwData.numObjOut = mmwData.header.numDetectedObj;
      }

      // RScan->header.seq = 0;
      // RScan->header.stamp = rclcpp::Clock().now();
      // RScan->header.stamp = (uint32_t) mmwData.header.timeCpuCycles;
      RScan->header.frame_id = frameID;
      RScan->height = 1;
      RScan->width = mmwData.numObjOut;
      RScan->is_dense = 1;
      RScan->points.resize(RScan->width * RScan->height);

      // Calculate ratios for max desired elevation and azimuth angles
      if ((maxAllowedElevationAngleDeg >= 0) &&
          (maxAllowedElevationAngleDeg < 90)) {
        maxElevationAngleRatioSquared =
            tan(maxAllowedElevationAngleDeg * M_PI / 180.0);
        maxElevationAngleRatioSquared =
            maxElevationAngleRatioSquared * maxElevationAngleRatioSquared;
      } else
        maxElevationAngleRatioSquared = -1;
      if ((maxAllowedAzimuthAngleDeg >= 0) && (maxAllowedAzimuthAngleDeg < 90))
        maxAzimuthAngleRatio = tan(maxAllowedAzimuthAngleDeg * M_PI / 180.0);
      else
        maxAzimuthAngleRatio = -1;

      // ROS_INFO("maxElevationAngleRatioSquared = %f",
      // maxElevationAngleRatioSquared); ROS_INFO("maxAzimuthAngleRatio = %f",
      // maxAzimuthAngleRatio); ROS_INFO("mmwData.numObjOut before = %d",
      // mmwData.numObjOut);

      // Populate pointcloud
      while (i < mmwData.numObjOut) {
        if (((mmwData.header.version >> 24) & 0xFF) <
            3) { // SDK version is older than 3.x
          // get object range index
          memcpy(&mmwData.objOut.rangeIdx, &currentBufp->at(currentDatap),
                 sizeof(mmwData.objOut.rangeIdx));
          currentDatap += (sizeof(mmwData.objOut.rangeIdx));

          // get object doppler index
          memcpy(&mmwData.objOut.dopplerIdx, &currentBufp->at(currentDatap),
                 sizeof(mmwData.objOut.dopplerIdx));
          currentDatap += (sizeof(mmwData.objOut.dopplerIdx));

          // get object peak intensity value
          memcpy(&mmwData.objOut.peakVal, &currentBufp->at(currentDatap),
                 sizeof(mmwData.objOut.peakVal));
          currentDatap += (sizeof(mmwData.objOut.peakVal));

          // get object x-coordinate
          memcpy(&mmwData.objOut.x, &currentBufp->at(currentDatap),
                 sizeof(mmwData.objOut.x));
          currentDatap += (sizeof(mmwData.objOut.x));

          // get object y-coordinate
          memcpy(&mmwData.objOut.y, &currentBufp->at(currentDatap),
                 sizeof(mmwData.objOut.y));
          currentDatap += (sizeof(mmwData.objOut.y));

          // get object z-coordinate
          memcpy(&mmwData.objOut.z, &currentBufp->at(currentDatap),
                 sizeof(mmwData.objOut.z));
          currentDatap += (sizeof(mmwData.objOut.z));

          float temp[7];

          temp[0] = (float)mmwData.objOut.x;
          temp[1] = (float)mmwData.objOut.y;
          temp[2] = (float)mmwData.objOut.z;
          temp[3] = (float)mmwData.objOut.dopplerIdx;

          for (int j = 0; j < 4; j++) {
            if (temp[j] > 32767)
              temp[j] -= 65536;
            if (j < 3)
              temp[j] = temp[j] / pow(2, mmwData.xyzQFormat);
          }

          temp[7] = temp[3] * vvel;

          temp[4] = (float)mmwData.objOut.rangeIdx * vrange;
          temp[5] = 10 * log10(mmwData.objOut.peakVal + 1); // intensity
          temp[6] = std::atan2(-temp[0], temp[1]) / M_PI * 180;

          uint16_t tmp = (uint16_t)(temp[3] + nd / 2);

          // Map mmWave sensor coordinates to ROS coordinate system
          RScan->points[i].x =
              temp[1]; // ROS standard coordinate system X-axis is forward which
                       // is the mmWave sensor Y-axis
          RScan->points[i].y =
              -temp[0]; // ROS standard coordinate system Y-axis is left which
                        // is the mmWave sensor -(X-axis)
          RScan->points[i].z =
              temp[2]; // ROS standard coordinate system Z-axis is up which is
                       // the same as mmWave sensor Z-axis
          RScan->points[i].intensity = temp[5];

          radarscan.header.frame_id = frameID;
          radarscan.header.stamp = rclcpp::Clock().now();

          radarscan.point_id = i;
          radarscan.x = temp[1];
          radarscan.y = -temp[0];
          radarscan.z = temp[2];
          radarscan.range = temp[4];
          radarscan.velocity = temp[7];
          radarscan.doppler_bin = tmp;
          radarscan.bearing = temp[6];
          radarscan.intensity = temp[5];
        } else { // SDK version is 3.x+
          // get object x-coordinate (meters)
          memcpy(&mmwData.newObjOut.x, &currentBufp->at(currentDatap),
                 sizeof(mmwData.newObjOut.x));
          currentDatap += (sizeof(mmwData.newObjOut.x));

          // get object y-coordinate (meters)
          memcpy(&mmwData.newObjOut.y, &currentBufp->at(currentDatap),
                 sizeof(mmwData.newObjOut.y));
          currentDatap += (sizeof(mmwData.newObjOut.y));

          // get object z-coordinate (meters)
          memcpy(&mmwData.newObjOut.z, &currentBufp->at(currentDatap),
                 sizeof(mmwData.newObjOut.z));
          currentDatap += (sizeof(mmwData.newObjOut.z));

          // get object velocity (m/s)
          memcpy(&mmwData.newObjOut.velocity, &currentBufp->at(currentDatap),
                 sizeof(mmwData.newObjOut.velocity));
          currentDatap += (sizeof(mmwData.newObjOut.velocity));

          // Map mmWave sensor coordinates to ROS coordinate system
          RScan->points[i].x =
              mmwData.newObjOut.y; // ROS standard coordinate system X-axis is
                                   // forward which is the mmWave sensor Y-axis
          RScan->points[i].y =
              -mmwData.newObjOut.x; // ROS standard coordinate system Y-axis is
                                    // left which is the mmWave sensor -(X-axis)
          RScan->points[i].z =
              mmwData.newObjOut
                  .z; // ROS standard coordinate system Z-axis is up which is
                      // the same as mmWave sensor Z-axis

          radarscan.header.frame_id = frameID;
          radarscan.header.stamp = rclcpp::Clock().now();

          radarscan.point_id = i;
          radarscan.x = mmwData.newObjOut.y;
          radarscan.y = -mmwData.newObjOut.x;
          radarscan.z = mmwData.newObjOut.z;
          // radarscan.range = temp[4];
          radarscan.velocity = mmwData.newObjOut.velocity;
          // radarscan.doppler_bin = tmp;
          // radarscan.bearing = temp[6];
          // radarscan.intensity = temp[5];

          // For SDK 3.x, intensity is replaced by snr in sideInfo and is parsed
          // in the READ_SIDE_INFO code
        }

        if (((maxElevationAngleRatioSquared == -1) ||
             (((RScan->points[i].z * RScan->points[i].z) /
               (RScan->points[i].x * RScan->points[i].x +
                RScan->points[i].y * RScan->points[i].y)) <
              maxElevationAngleRatioSquared)) &&
            ((maxAzimuthAngleRatio == -1) ||
             (fabs(RScan->points[i].y / RScan->points[i].x) <
              maxAzimuthAngleRatio)) &&
            (RScan->points[i].x != 0)) {
          radar_scan_pub->publish(radarscan);
        }
        i++;
      }

      sorterState = CHECK_TLV_TYPE;

      break;

    case READ_SIDE_INFO:

      // Make sure we already received and parsed detected obj list
      // (READ_OBJ_STRUCT)
      if (mmwData.numObjOut > 0) {
        for (i = 0; i < mmwData.numObjOut; i++) {
          // get snr (unit is 0.1 steps of dB)
          memcpy(&mmwData.sideInfo.snr, &currentBufp->at(currentDatap),
                 sizeof(mmwData.sideInfo.snr));
          currentDatap += (sizeof(mmwData.sideInfo.snr));

          // get noise (unit is 0.1 steps of dB)
          memcpy(&mmwData.sideInfo.noise, &currentBufp->at(currentDatap),
                 sizeof(mmwData.sideInfo.noise));
          currentDatap += (sizeof(mmwData.sideInfo.noise));

          RScan->points[i].intensity =
              (float)mmwData.sideInfo.snr /
              10.0; // Use snr for "intensity" field (divide by 10 since unit of
                    // snr is 0.1dB)
        }
      } else // else just skip side info section if we have not already received
             // and parsed detected obj list
      {
        i = 0;

        while (i++ < tlvLen - 1) {
          // ROS_INFO("DataUARTHandler Sort Thread : Parsing Side Info i=%d and
          // tlvLen = %u", i, tlvLen);
        }

        currentDatap += tlvLen;
      }

      sorterState = CHECK_TLV_TYPE;

      break;

    case READ_LOG_MAG_RANGE:
      sorterState = CHECK_TLV_TYPE;
      break;

    case READ_NOISE:

      i = 0;

      while (i++ < tlvLen - 1) {
        // ROS_INFO("DataUARTHandler Sort Thread : Parsing Noise Profile i=%d
        // and tlvLen = %u", i, tlvLen);
      }

      currentDatap += tlvLen;

      sorterState = CHECK_TLV_TYPE;
      break;

    case READ_AZIMUTH:

      i = 0;

      while (i++ < tlvLen - 1) {
        // ROS_INFO("DataUARTHandler Sort Thread : Parsing Azimuth Profile i=%d
        // and tlvLen = %u", i, tlvLen);
      }

      currentDatap += tlvLen;

      sorterState = CHECK_TLV_TYPE;
      break;

    case READ_DOPPLER:

      i = 0;

      while (i++ < tlvLen - 1) {
        // ROS_INFO("DataUARTHandler Sort Thread : Parsing Doppler Profile i=%d
        // and tlvLen = %u", i, tlvLen);
      }

      currentDatap += tlvLen;

      sorterState = CHECK_TLV_TYPE;
      break;

    case READ_STATS:

      i = 0;

      while (i++ < tlvLen - 1) {
        // ROS_INFO("DataUARTHandler Sort Thread : Parsing Stats Profile i=%d
        // and tlvLen = %u", i, tlvLen);
      }

      currentDatap += tlvLen;

      sorterState = CHECK_TLV_TYPE;
      break;

    case CHECK_TLV_TYPE:

      // ROS_INFO("DataUARTHandler Sort Thread : tlvCount = %d, numTLV = %d",
      // tlvCount, mmwData.header.numTLVs);

      if (tlvCount++ >=
          mmwData.header.numTLVs) // Done parsing all received TLV sections
      {
        // Publish detected object pointcloud
        if (mmwData.numObjOut > 0) {
          j = 0;
          for (i = 0; i < mmwData.numObjOut; i++) {
            // Keep point if elevation and azimuth angles are less than
            // specified max values (NOTE: The following calculations are done
            // using ROS standard coordinate system axis definitions where X is
            // forward and Y is left)
            if (((maxElevationAngleRatioSquared == -1) ||
                 (((RScan->points[i].z * RScan->points[i].z) /
                   (RScan->points[i].x * RScan->points[i].x +
                    RScan->points[i].y * RScan->points[i].y)) <
                  maxElevationAngleRatioSquared)) &&
                ((maxAzimuthAngleRatio == -1) ||
                 (fabs(RScan->points[i].y / RScan->points[i].x) <
                  maxAzimuthAngleRatio)) &&
                (RScan->points[i].x != 0)) {
              // ROS_INFO("Kept point");
              // copy: points[i] => points[j]
              memcpy(&RScan->points[j], &RScan->points[i],
                     sizeof(RScan->points[i]));
              j++;
            }
          }
          mmwData.numObjOut = j; // update number of objects as some points may
                                 // have been removed

          // Resize point cloud since some points may have been removed
          RScan->width = mmwData.numObjOut;
          RScan->points.resize(RScan->width * RScan->height);

          // ROS_INFO("mmwData.numObjOut after = %d", mmwData.numObjOut);
          // ROS_INFO("DataUARTHandler Sort Thread: number of obj = %d",
          // mmwData.numObjOut );
          pcl::PCLPointCloud2 cloud_ROI;
          pcl::toPCLPointCloud2(*RScan, cloud_ROI);
          pcl_conversions::fromPCL(cloud_ROI, output_pointcloud);
          DataUARTHandler_pub->publish(output_pointcloud);
        }

        // ROS_INFO("DataUARTHandler Sort Thread : CHECK_TLV_TYPE state says
        // tlvCount max was reached, going to switch buffer state");
        sorterState = SWAP_BUFFERS;
      }

      else // More TLV sections to parse
      {
        // get tlvType (32 bits) & remove from queue
        memcpy(&tlvType, &currentBufp->at(currentDatap), sizeof(tlvType));
        currentDatap += (sizeof(tlvType));

        // ROS_INFO("DataUARTHandler Sort Thread : sizeof(tlvType) = %d",
        // sizeof(tlvType));

        // get tlvLen (32 bits) & remove from queue
        memcpy(&tlvLen, &currentBufp->at(currentDatap), sizeof(tlvLen));
        currentDatap += (sizeof(tlvLen));

        // ROS_INFO("DataUARTHandler Sort Thread : sizeof(tlvLen) = %d",
        // sizeof(tlvLen));

        // ROS_INFO("DataUARTHandler Sort Thread : tlvType = %d, tlvLen = %d",
        // (int) tlvType, tlvLen);

        switch (tlvType) {
        case MMWDEMO_OUTPUT_MSG_NULL:

          break;

        case MMWDEMO_OUTPUT_MSG_DETECTED_POINTS:
          // ROS_INFO("DataUARTHandler Sort Thread : Object TLV");
          sorterState = READ_OBJ_STRUCT;
          break;

        case MMWDEMO_OUTPUT_MSG_RANGE_PROFILE:
          // ROS_INFO("DataUARTHandler Sort Thread : Range TLV");
          sorterState = READ_LOG_MAG_RANGE;
          break;

        case MMWDEMO_OUTPUT_MSG_NOISE_PROFILE:
          // ROS_INFO("DataUARTHandler Sort Thread : Noise TLV");
          sorterState = READ_NOISE;
          break;

        case MMWDEMO_OUTPUT_MSG_AZIMUTH_STATIC_HEAT_MAP:
          // ROS_INFO("DataUARTHandler Sort Thread : Azimuth Heat TLV");
          sorterState = READ_AZIMUTH;
          break;

        case MMWDEMO_OUTPUT_MSG_RANGE_DOPPLER_HEAT_MAP:
          // ROS_INFO("DataUARTHandler Sort Thread : R/D Heat TLV");
          sorterState = READ_DOPPLER;
          break;

        case MMWDEMO_OUTPUT_MSG_STATS:
          // ROS_INFO("DataUARTHandler Sort Thread : Stats TLV");
          sorterState = READ_STATS;
          break;

        case MMWDEMO_OUTPUT_MSG_DETECTED_POINTS_SIDE_INFO:
          // ROS_INFO("DataUARTHandler Sort Thread : Side info TLV");
          sorterState = READ_SIDE_INFO;
          break;

        case MMWDEMO_OUTPUT_MSG_MAX:
          // ROS_INFO("DataUARTHandler Sort Thread : Header TLV");
          sorterState = READ_HEADER;
          break;

        default:
          break;
        }
      }

      break;

    case SWAP_BUFFERS:

      pthread_mutex_lock(&countSync_mutex);
      pthread_mutex_unlock(&currentBufp_mutex);

      countSync++;

      if (countSync == COUNT_SYNC_MAX) {
        pthread_cond_signal(&countSync_max_cv);
      }

      pthread_cond_wait(&sort_go_cv, &countSync_mutex);

      pthread_mutex_unlock(&countSync_mutex);
      pthread_mutex_lock(&currentBufp_mutex);

      currentDatap = 0;
      tlvCount = 0;

      sorterState = READ_HEADER;

      break;

    default:
      break;
    }
  }

  pthread_exit(NULL);
}

void DataUARTHandler::start(void) {

  pthread_t uartThread, sorterThread, swapThread;

  int iret1, iret2, iret3;

  pthread_mutex_init(&countSync_mutex, NULL);
  pthread_mutex_init(&nextBufp_mutex, NULL);
  pthread_mutex_init(&currentBufp_mutex, NULL);
  pthread_cond_init(&countSync_max_cv, NULL);
  pthread_cond_init(&read_go_cv, NULL);
  pthread_cond_init(&sort_go_cv, NULL);

  countSync = 0;

  /* Create independent threads each of which will execute function */
  iret1 =
      pthread_create(&uartThread, NULL, this->readIncomingData_helper, this);
  if (iret1) {
    printf("Error - pthread_create() return code: %d\n", iret1);
    rclcpp::shutdown();
  }

  iret2 =
      pthread_create(&sorterThread, NULL, this->sortIncomingData_helper, this);
  if (iret2) {
    printf("Error - pthread_create() return code: %d\n", iret2);
    rclcpp::shutdown();
  }

  iret3 =
      pthread_create(&swapThread, NULL, this->syncedBufferSwap_helper, this);
  if (iret3) {
    printf("Error - pthread_create() return code: %d\n", iret3);
    rclcpp::shutdown();
  }

  // rclcpp::spin(shared_from_this());
  while (1)
    continue;

  pthread_join(iret1, NULL);
  printf("DataUARTHandler Read Thread joined\n");
  pthread_join(iret2, NULL);
  printf("DataUARTHandler Sort Thread joined\n");
  pthread_join(iret3, NULL);
  printf("DataUARTHandler Swap Thread joined\n");

  pthread_mutex_destroy(&countSync_mutex);
  pthread_mutex_destroy(&nextBufp_mutex);
  pthread_mutex_destroy(&currentBufp_mutex);
  pthread_cond_destroy(&countSync_max_cv);
  pthread_cond_destroy(&read_go_cv);
  pthread_cond_destroy(&sort_go_cv);
}

void *DataUARTHandler::readIncomingData_helper(void *context) {
  return (static_cast<DataUARTHandler *>(context)->readIncomingData());
}

void *DataUARTHandler::sortIncomingData_helper(void *context) {
  return (static_cast<DataUARTHandler *>(context)->sortIncomingData());
}

void *DataUARTHandler::syncedBufferSwap_helper(void *context) {
  return (static_cast<DataUARTHandler *>(context)->syncedBufferSwap());
}

void DataUARTHandler::visualize(
    const ti_mmwave_ros2_interfaces::msg::RadarScan &msg) {
  // visualization_msgs::msg::Marker marker;
  auto marker = visualization_msgs::msg::Marker();

  marker.header.frame_id = frameID;
  marker.header.stamp = rclcpp::Clock().now();
  marker.id = msg.point_id;
  marker.type = visualization_msgs::msg::Marker::SPHERE;
  marker.lifetime = rclcpp::Duration(tfr, 0);
  marker.action = marker.ADD;

  marker.pose.position.x = msg.x;
  marker.pose.position.y = msg.y;
  marker.pose.position.z = 0;

  marker.pose.orientation.x = 0;
  marker.pose.orientation.y = 0;
  marker.pose.orientation.z = 0;
  marker.pose.orientation.w = 0;

  marker.scale.x = .03;
  marker.scale.y = .03;
  marker.scale.z = .03;

  marker.color.a = 1;
  marker.color.r = (int)255 * msg.intensity;
  marker.color.g = (int)255 * msg.intensity;
  marker.color.b = 1;

  marker_pub->publish(marker);
}
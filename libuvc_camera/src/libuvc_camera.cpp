/** @file test_ros_ctrls.cpp Example/test usage of libuvc */
#include <ros/ros.h>
#include <sensor_msgs/Image.h>
#include "libuvc/libuvc.h"

using std::string;

ros::Publisher pub;

void cb(uvc_frame_t *frame) {
  static uvc_frame_t *rgb_frame = NULL;
  uvc_error_t uvc_ret;

  if (!rgb_frame)
    rgb_frame = uvc_allocate_frame(frame->width * frame->height * 3);

  uvc_ret = uvc_any2rgb(frame, rgb_frame);

  if (uvc_ret) {
    uvc_perror(uvc_ret, "Couldn't convert frame to RGB");
    return;
  }

  sensor_msgs::Image image;
  image.width = rgb_frame->width;
  image.height = rgb_frame->height;
  image.encoding = "rgb8";
  image.step = image.width * 3;
  image.data.resize(image.step * image.height);
  memcpy(&(image.data[0]), rgb_frame->data, rgb_frame->data_bytes);

  pub.publish(image);
}

int main (int argc, char **argv) {
  ros::init(argc, argv, "libuvc_camera");
  ros::NodeHandle nh;
  ros::NodeHandle priv_nh("~");

  pub = nh.advertise<sensor_msgs::Image>("image_raw", 1);

  uvc_context_t *ctx;
  uvc_error_t res;
  uvc_device_t *dev;
  uvc_device_handle_t *devh;
  uvc_stream_ctrl_t ctrl;

  res = uvc_init(&ctx, NULL);

  if (res < 0) {
    uvc_perror(res, "uvc_init");
    return res;
  }

  puts("UVC initialized");

  int vendor_id;
  string vendor_id_str;
  priv_nh.getParam("vendor", vendor_id_str);
  vendor_id = strtol(vendor_id_str.c_str(), NULL, 0);

  int product_id;
  string product_id_str;
  priv_nh.getParam("product", product_id_str);
  product_id = strtol(product_id_str.c_str(), NULL, 0);

  string serial_num;
  priv_nh.getParam("serial", serial_num);

  printf("pid: %d\n", product_id);
  printf("vid: %d\n", vendor_id);
  
  res = uvc_find_device(
    ctx, &dev,
    vendor_id,
    product_id,
    serial_num.empty() ? NULL : serial_num.c_str());

  if (res < 0) {
    uvc_perror(res, "uvc_find_device");
  } else {
    res = uvc_open(dev, &devh);
    if (res < 0) {
      uvc_perror(res, "uvc_open");
    } else {
      puts("Device opened");
      
      uvc_print_diag(devh, stderr);
      
      res = uvc_get_stream_ctrl_format_size(
        devh, &ctrl,
        UVC_COLOR_FORMAT_UNCOMPRESSED, 640, 480, 30);
      
      uvc_print_stream_ctrl(&ctrl, stderr);
      
      if (res < 0) {
	uvc_perror(res, "get_mode");
      } else {
        res = uvc_start_iso_streaming(devh, &ctrl, cb);

        if (res < 0) {
          uvc_perror(res, "start_streaming");
        } else {
          puts("Streaming...");
          ros::spin();
          uvc_stop_streaming(devh);
        }
      }
      
      uvc_close(devh);
      puts("Device closed");
    }
  }
  
  uvc_exit(ctx);
  puts("UVC exited");
  
  return 0;
}

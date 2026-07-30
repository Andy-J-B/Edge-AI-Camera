#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/init.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <media/v4l2-dev.h>
#include <media/v4l2-device.h>

// 1. Private device structure holding driver state
struct edge_cam_dev {
  struct platform_device *pdev;
  struct v4l2_device v4l2_dev;
  struct video_device vdev;
};

// 2. Minimal V4L2 File Operations
static const struct v4l2_file_operations edge_cam_fops = {
    .owner = THIS_MODULE,
    .open = v4l2_fh_open,
    .release = vb2_fop_release,
    .unlocked_ioctl = video_ioctl2,
};

// 3. Platform Probe Callback (Called when device matches driver)
static int edge_cam_probe(struct platform_device *pdev) {
  struct edge_cam_dev *cam;
  int ret;

  pr_info("Probing Edge AI Camera platform driver...\n");

  cam = devm_kzalloc(&pdev->dev, sizeof(*cam), GFP_KERNEL);
  if (!cam)
    return -ENOMEM;

  cam->pdev = pdev;
  platform_set_drvdata(pdev, cam);

  // Register parent V4L2 device
  ret = v4l2_device_register(&pdev->dev, &cam->v4l2_dev);
  if (ret)
    return ret;

  // Configure /dev/videoX node
  snprintf(cam->vdev.name, sizeof(cam->vdev.name), "edge-ai-cam");
  cam->vdev.fops = &edge_cam_fops;
  cam->vdev.v4l2_dev = &cam->v4l2_dev;
  cam->vdev.release = video_device_release_empty;
  cam->vdev.device_caps = V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_STREAMING;

  // Register video device (creates /dev/videoX)
  ret = video_register_device(&cam->vdev, VFL_TYPE_VIDEO, -1);
  if (ret) {
    v4l2_device_unregister(&cam->v4l2_dev);
    return ret;
  }

  pr_info("Registered /dev/video%d\n", cam->vdev.num);
  return 0;
}

// 4. Platform Remove Callback
static void edge_cam_remove(struct platform_device *pdev) {
  struct edge_cam_dev *cam = platform_get_drvdata(pdev);

  video_unregister_device(&cam->vdev);
  v4l2_device_unregister(&cam->v4l2_dev);
  pr_info("Unregistered Edge AI Camera\n");
}

// 5. Device Tree Matching Table
static const struct of_device_id edge_cam_of_match[] = {
    {
        .compatible = "vendor,edge-ai-camera",
    },
    {/* sentinel */}};
MODULE_DEVICE_TABLE(of, edge_cam_of_match);

// 6. Platform Driver Definition
static struct platform_driver edge_cam_driver = {
    .probe = edge_cam_probe,
    .remove = edge_cam_remove,
    .driver =
        {
            .name = "edge_ai_camera",
            .of_match_table = edge_cam_of_match,
        },
};

module_platform_driver(edge_cam_driver);

MODULE_LICENSE("GPL");

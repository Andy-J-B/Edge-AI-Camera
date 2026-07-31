#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <media/v4l2-dev.h>
#include <media/v4l2-device.h>
#include <media/v4l2-fh.h>
#include <media/videobuf2-dma-contig.h>
#include <media/videobuf2-v4l2.h>

#define DEFAULT_WIDTH 640
#define DEFAULT_HEIGHT 360
#define DEFAULT_BPP 1
#define DEFAULT_IMAGE_SIZE (DEFAULT_WIDTH * DEFAULT_HEIGHT * DEFAULT_BPP)
// 640x360 @ 1 byte per pixel

struct edge_cam_buffer {
  struct vb2_v4l2_buffer vb;
  struct list_head list;
}

// 1. Private device structure
struct edge_cam_dev {
  struct platform_device *pdev;
  struct v4l2_device v4l2_dev;
  struct video_device vdev;
  struct vb2_queue queue;

  struct list_head dma_queue; // list of active queued buffers awaiting HW DMA
  spinlock_t qlock;           // protect dma_queue

  u32 width;
  u32 height;
  u32 sizeimage;
  u32 sequence; // frame counter
};

// Called when userspace calls VIDIOC_REQBUFS
// Asking for buffer allocations
static int edge_cam_queue_setup(struct vb2_queue *vq, unsigned int *nbuffers,
                                unsigned int *nplanes, unsigned int sizes[],
                                struct device *alloc_devs[]) {
  struct edge_cam_dev *cam = vb2_get_drvpriv(vq);

  // V4L2 single-planar formats have 1 plane
  if (*nplanes)
    return sizes[0] < cam->sizeimage ? -EINVAL : 0;

  *nplanes = 1;
  sizes[0] = cam->sizeimage;

  // Ensure a reasonable minimum number of buffers (e.g., 2)
  if (*nbuffers < 2)
    *nbuffers = 2;

  pr_info("vb2_queue_setup: %u buffers allocated, size = %u bytes\n", *nbuffers,
          sizes[0]);

  return 0;
}

static int edge_cam_buf_prepare(struct vb2_buffer *vb) {
  struct edge_cam_dev *cam = vb2_get_drvpriv(vb->vb2_queue);

  if (vb2_plane_size(vb, 0) < cam->sizeimage) {
    pr_err("Buffer size (%lu) smaller than required image size (%u)\n",
           vb2_plane_size(vb, 0), cam->sizeimage);
    return -EINVAL;
  }

  vb2_set_plane_payload(vb, 0, cam->sizeimage);
  return 0;
}

static void edge_cam_buf_queue(struct vb2_buffer *vb) {
  struct edge_cam_dev *cam = vb2_get_drvpriv(vb->vb2_queue);
  struct vb2_v4l2_buffer *vbuf = to_vb2_v4l2_buffer(vb);
  struct edge_cam_buffer *buf = container_of(vbuf, struct edge_cam_buffer, vb);
  unsigned long flags;

  spin_lock_irqsave(&cam->qlock, flags);
  list_add_tail(&buf->list, &cam->dma_queue);
  spin_unlock_irqrestore(&cam->qlock, flags);
}

static int edge_cam_start_streaming(struct vb2_queue *vq, unsigned int count) {
  struct edge_cam_dev *cam = vb2_get_drvpriv(vq);

  cam->sequence = 0;
  pr_info("Stream started!\n");
  return 0;
}

static void edge_cam_stop_streaming(struct vb2_queue *vq) {
  struct edge_cam_dev *cam = vb2_get_drvpriv(vq);
  struct edge_cam_buffer *buf, *node;
  unsigned long flags;

  pr_info("Stream stopped!\n");

  spin_lock_irqsave(&cam->qlock, flags);
  list_for_each_entry_safe(buf, node, &cam->dma_queue, list) {
    list_del(&buf->list);
    vb2_buffer_done(&buf->vb.vb2_buf, VB2_BUF_STATE_ERROR);
  }
  spin_unlock_irqrestore(&cam->qlock, flags);
}

// vb2_ops structure Table
static const struct vb2_ops edge_cam_qops =
    {
        .queue_setup = edge_cam_queue_setup,
        .buf_prepare = edge_cam_buf_prepare,
        .buf_queue = edge_cam_buf_queue,
        .start_streaming = edge_cam_start_streaming,
        .stop_streaming = edge_cam_stop_streaming,
        .wait_prepare = vb2_ops_wait_prepare,
        .wait_finish = vb2_ops_wait_finish,
}

// 2. V4L2 File Operations
static const struct v4l2_file_operations edge_cam_fops = {
    .owner = THIS_MODULE,
    .open = v4l2_fh_open,
    .release = vb2_fop_release,
    .read = vb2_vb2_fop_read,
    .mmap = vb2_fop_mmap,
    .unlocked_ioctl = video_ioctl2,
    .poll = vb2_fop_poll,
};

// 3. Platform Probe Callback (Called when device matches driver)
static int edge_cam_probe(struct platform_device *pdev) {
  struct edge_cam_dev *cam;
  struct vb2_queue *q;
  int ret;

  cam = devm_kzalloc(&pdev->dev, sizeof(*cam), GFP_KERNEL);
  if (!cam)
    return -ENOMEM;

  cam->pdev = pdev;
  cam->width = DEFAULT_WIDTH;
  cam->height = DEFAULT_HEIGHT;
  cam->sizeimage = DEFAULT_IMAGE_SIZE;

  INIT_LIST_HEAD(&cam->dma_queue);
  spin_lock_init(&cam->qlock);

  // Register parent V4L2 device
  ret = v4l2_device_register(&pdev->dev, &cam->v4l2_dev);
  if (ret)
    return ret;

  // --- INITIALIZE VB2 QUEUE ---
  q = &cam->queue;
  q->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  q->io_modes = VB2_MMAP | VB2_DMABUF | VB2_READ;
  q->drv_priv = cam;
  q->buf_struct_size = sizeof(struct edge_cam_buffer);
  q->ops = &edge_cam_qops;
  q->mem_ops = &vb2_dma_contig_memops; // Memory allocator helper
  q->timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_MONOTONIC;
  q->min_buffers_needed = 2;
  q->dev = &pdev->dev;

  ret = vb2_queue_init(q);
  if (ret) {
    pr_err("Failed to initialize vb2 queue\n");
    goto err_v4l2;
  }

  // --- CONFIGURE VIDEO DEVICE NODE ---
  snprintf(cam->vdev.name, sizeof(cam->vdev.name), "edge-ai-cam");
  cam->vdev.fops = &edge_cam_fops;
  cam->vdev.v4l2_dev = &cam->v4l2_dev;
  cam->vdev.queue = &cam->queue; // Attach queue to video node
  cam->vdev.release = video_device_release_empty;
  cam->vdev.device_caps =
      V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_STREAMING | V4L2_CAP_READWRITE;

  ret = video_register_device(&cam->vdev, VFL_TYPE_VIDEO, -1);
  if (ret)
    goto err_v4l2;

  pr_info("Registered /dev/video%d with vb2 queue support\n", cam->vdev.num);
  return 0;

err_v4l2:
  v4l2_device_unregister(&cam->v4l2_dev);
  return ret;
}

// 4. Platform Remove Callback
static void edge_cam_remove(struct platform_device *pdev) {
  struct edge_cam_dev *cam = platform_get_drvdata(pdev);

  video_unregister_device(&cam->vdev);
  v4l2_device_unregister(&cam->v4l2_dev);
  pr_info("Unregistered Edge AI Camera\n");
}

// 6. Platform Driver Definition
static struct platform_driver edge_cam_driver = {
    .probe = edge_cam_probe,
    .remove = edge_cam_remove,
    .driver =
        {
            .name = "edge_ai_camera",
        },
};

static struct platform_device *pdev_dummmy;

static int __init edge_cam_init(void) {
  int ret = platform_driver_register(&edge_cam_driver);
  if (ret)
    return ret;

  pdev_dummy = platform_device_register_simple("edge_ai_camera", -1, NULL, 0);
  if (IS_ERR(pdev_dummy)) {
    platform_driver_unregister(&edge_cam_driver);
    return PTR_ERR(pdev_dummy);
  }
  return 0;
}

static void __exit edge_cam_exit(void) {
  platform_device_unregister(pdev_dummy);
  platform_driver_unregister(&edge_cam_driver);
}

module_init(edge_cam_init);
module_exit(edge_cam_exit);

MODULE_LICENSE("GPL");

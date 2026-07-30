// Platform driver

#include <kernel>
#include <linux/init.h>
#include <linux/module.h>

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

static int __init edge_ai_camera_init(void) {
  pr_info("Hello, welcome to Edge AI Camera\n");
  return 0;
}

static void __exit edge_ai_camera_exit(void) {
  pr_info("Unloaded Edge AI Camera driver\n");
}

module_init(edge_ai_camera_init);
module_init(edge_ai_camera_exit);

MODULE_LICENSE("GPL");

// Platform driver

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/init.h>
#include <linux/module.h>

static int __init edge_ai_camera_init(void) {
  pr_info("Hello, welcome to Edge AI Camera\n");
  return 0;
}

static void __exit edge_ai_camera_exit(void) {
  pr_info("Unloaded Edge AI Camera driver\n");
}

module_init(edge_ai_camera_init);
module_exit(edge_ai_camera_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Junhyuk Andy Bae");
MODULE_DESCRIPTION("Edge AI Camera");

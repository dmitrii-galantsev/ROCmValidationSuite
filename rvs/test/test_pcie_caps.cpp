/********************************************************************************
 *
 * Copyright (c) 2018 ROCm Developer Tools
 *
 * MIT LICENSE:
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is furnished to do
 * so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 *******************************************************************************/

#include "gtest/gtest.h"

#include "pci_caps.h"
#include "rvs_unit_testing_defs.h"

#include <pci/pci.h>
#include <linux/pci.h>

#include <vector>

using namespace rvs;

class PcieCapsTest : public ::testing::Test {
 protected:
  void SetUp() override {
    test_dev = new pci_dev();

    test_cap[0] = new pci_cap();
    test_cap[1] = new pci_cap();

    // first cap
    test_cap[0]->id   = 17;
    test_cap[0]->type = 34;
    test_cap[0]->addr = 219;
    test_cap[0]->next = test_cap[1];
    // second cap
    test_cap[1]->id   = 25;
    test_cap[1]->type = 5;
    test_cap[1]->addr = 198;

    // fill up the struct
    test_dev->first_cap = test_cap[0];
    test_dev->bus = 9;
    test_dev->device_id = 54;
    test_dev->vendor_id = 98;
  }

  void TearDown() override {

  }

  // pci device struct
  pci_dev* test_dev;

  // pci caps struct
  pci_cap* test_cap[2];

  // utility function
  void get_num_bits(uint32_t input_num, uint32_t* num_ones, int* first_one, uint32_t* max_value) {
    *num_ones = 0;
    *first_one = -1;
    for (int i = 0; i < 32; i++) {
      if (((input_num >> i) & 0x1) == 1) {
        (*num_ones)++;
        if (*first_one == -1) {
          *first_one = i;
        }
      }
    }
    *max_value = (1 << *num_ones);
  }
  
};

TEST_F(PcieCapsTest, pcie_caps) {
  int return_value;
  char* buff;
  std::string exp_string;
  uint32_t num_ones;
  int first_one;
  uint32_t max_value;

  get_num_bits(0xf, &num_ones, &first_one, &max_value);
  EXPECT_EQ(num_ones, 4);
  EXPECT_EQ(first_one, 0);
  EXPECT_EQ(max_value, 16);

  get_num_bits(0x3f0, &num_ones, &first_one, &max_value);
  EXPECT_EQ(num_ones, 6);
  EXPECT_EQ(first_one, 4);
  EXPECT_EQ(max_value, 64);

  // ---------------------------------------
  // pci_dev_find_cap_offset
  // ---------------------------------------
  // 1. null ptr
  test_dev->first_cap = NULL;
  return_value = pci_dev_find_cap_offset(test_dev, 0, 0);
  EXPECT_EQ(return_value, 0);
  test_dev->first_cap = test_cap[0];
  for (int i = 0; i < 2; i++) {
    // 2. both correct
    return_value = pci_dev_find_cap_offset(test_dev, test_cap[i]->id, test_cap[i]->type);
    EXPECT_EQ(return_value, test_cap[i]->addr);
    // 3. one correct
    return_value = pci_dev_find_cap_offset(test_dev, test_cap[i]->id, 0);
    EXPECT_EQ(return_value, 0);
    return_value = pci_dev_find_cap_offset(test_dev, 0, test_cap[i]->type);
    EXPECT_EQ(return_value, 0);
  }
  // 4. none correct
  return_value = pci_dev_find_cap_offset(test_dev, 0, 0);
  EXPECT_EQ(return_value, 0);

  // ---------------------------------------
  // get_link_cap_max_speed
  // ---------------------------------------
  // 1. invalid Id / Type (Id = PCI_CAP_ID_EXP and Type = PCI_CAP_NORMAL are valid)
  for (int id_f = 0; id_f < 2; id_f++) {
    for (int type_f = 0; type_f < 2; type_f++) {
      test_cap[id_f]->id           = 0;
      test_cap[type_f]->type       = 0;
      test_cap[(id_f+1)%2]->id     = PCI_CAP_ID_EXP;
      test_cap[(type_f+1)%2]->type = PCI_CAP_NORMAL;
      rvs_pci_read_long_return_value = 15;
      buff = new char[1024];
      get_link_cap_max_speed(test_dev, buff);

      if (id_f == type_f) {
        // other one is valid
        EXPECT_STREQ(buff, "Unknown speed");
      } else {
        // none is valid
        EXPECT_STREQ(buff, "NOT SUPPORTED");
      }
    }
  }
  // 2. valid Id / Type values and iterate rvs_pci_read_long_return_value
  test_cap[0]->id   = PCI_CAP_ID_EXP;
  test_cap[0]->type = PCI_CAP_NORMAL;
  test_cap[1]->id   = PCI_CAP_ID_EXP;
  test_cap[1]->type = PCI_CAP_NORMAL;
  get_num_bits(PCI_EXP_LNKCAP_SLS, &num_ones, &first_one, &max_value);
  for (int k = 0; k < max_value; k++) {
    buff = new char[1024];
    rvs_pci_read_long_return_value = (k << first_one);
    get_link_cap_max_speed(test_dev, buff);
    rvs_pci_read_long_return_value = k;
    switch (rvs_pci_read_long_return_value) {
    case 1:
      EXPECT_STREQ(buff, "2.5 GT/s");
      break;
    case 2:
      EXPECT_STREQ(buff, "5 GT/s");
      break;
    case 3:
      EXPECT_STREQ(buff, "8 GT/s");
      break;
    case 4:
      EXPECT_STREQ(buff, "16 GT/s");
      break;
    default:
      EXPECT_STREQ(buff, "Unknown speed");
      break;
    }
  }

  // ---------------------------------------
  // get_link_cap_max_width
  // ---------------------------------------
  // 1. invalid Id / Type (Id = PCI_CAP_ID_EXP and Type = PCI_CAP_NORMAL are valid)
  for (int id_f = 0; id_f < 2; id_f++) {
    for (int type_f = 0; type_f < 2; type_f++) {
      test_cap[id_f]->id           = 0;
      test_cap[type_f]->type       = 0;
      test_cap[(id_f+1)%2]->id     = PCI_CAP_ID_EXP;
      test_cap[(type_f+1)%2]->type = PCI_CAP_NORMAL;
      rvs_pci_read_long_return_value = 15;
      buff = new char[1024];
      get_link_cap_max_width(test_dev, buff);

      if (id_f == type_f) {
        // other one is valid
        continue;
      } else {
        // none is valid
        EXPECT_STREQ(buff, "NOT SUPPORTED");
      }
    }
  }
  // 2. valid Id / Type values and iterate rvs_pci_read_long_return_value
  test_cap[0]->id   = PCI_CAP_ID_EXP;
  test_cap[0]->type = PCI_CAP_NORMAL;
  test_cap[1]->id   = PCI_CAP_ID_EXP;
  test_cap[1]->type = PCI_CAP_NORMAL;
  get_num_bits(PCI_EXP_LNKCAP_MLW, &num_ones, &first_one, &max_value);
  for (int k = 0; k < max_value; k++) {
    rvs_pci_read_long_return_value = (k << first_one);
    buff = new char[1024];
    get_link_cap_max_width(test_dev, buff);
    exp_string = "x" + std::to_string(k);
    EXPECT_STREQ(buff, exp_string.c_str());
  }

  // ---------------------------------------
  // get_link_stat_cur_speed
  // ---------------------------------------
  // 1. invalid Id / Type (Id = PCI_CAP_ID_EXP and Type = PCI_CAP_NORMAL are valid)
  for (int id_f = 0; id_f < 2; id_f++) {
    for (int type_f = 0; type_f < 2; type_f++) {
      test_cap[id_f]->id           = 0;
      test_cap[type_f]->type       = 0;
      test_cap[(id_f+1)%2]->id     = PCI_CAP_ID_EXP;
      test_cap[(type_f+1)%2]->type = PCI_CAP_NORMAL;
      rvs_pci_read_word_return_value = 15;
      buff = new char[1024];
      get_link_stat_cur_speed(test_dev, buff);

      if (id_f == type_f) {
        // other one is valid
        EXPECT_STREQ(buff, "Unknown speed");
      } else {
        // none is valid
        EXPECT_STREQ(buff, "NOT SUPPORTED");
      }
    }
  }
  // 2. valid Id / Type values and iterate rvs_pci_read_word_return_value
  test_cap[0]->id   = PCI_CAP_ID_EXP;
  test_cap[0]->type = PCI_CAP_NORMAL;
  test_cap[1]->id   = PCI_CAP_ID_EXP;
  test_cap[1]->type = PCI_CAP_NORMAL;
  get_num_bits(PCI_EXP_LNKSTA_CLS, &num_ones, &first_one, &max_value);
  for (int k = 0; k < max_value; k++) {
    buff = new char[1024];
    rvs_pci_read_word_return_value = (k << first_one);
    get_link_stat_cur_speed(test_dev, buff);
    rvs_pci_read_word_return_value = k;
    switch (rvs_pci_read_word_return_value) {
    case 1:
      EXPECT_STREQ(buff, "2.5 GT/s");
      break;
    case 2:
      EXPECT_STREQ(buff, "5 GT/s");
      break;
    case 3:
      EXPECT_STREQ(buff, "8 GT/s");
      break;
#ifdef PCI_EXP_LNKSTA_CLS_16_0GB
    case 4:
      EXPECT_STREQ(buff, "16 GT/s");
      break;
#endif
    default:
      EXPECT_STREQ(buff, "Unknown speed");
      break;
    }
  }

  // ---------------------------------------
  // get_link_stat_neg_width
  // ---------------------------------------
  // 1. invalid Id / Type (Id = PCI_CAP_ID_EXP and Type = PCI_CAP_NORMAL are valid)
  for (int id_f = 0; id_f < 2; id_f++) {
    for (int type_f = 0; type_f < 2; type_f++) {
      test_cap[id_f]->id           = 0;
      test_cap[type_f]->type       = 0;
      test_cap[(id_f+1)%2]->id     = PCI_CAP_ID_EXP;
      test_cap[(type_f+1)%2]->type = PCI_CAP_NORMAL;
      rvs_pci_read_word_return_value = 15;
      buff = new char[1024];
      get_link_stat_neg_width(test_dev, buff);

      if (id_f == type_f) {
        // other one is valid
        continue;
      } else {
        // none is valid
        EXPECT_STREQ(buff, "NOT SUPPORTED");
      }
    }
  }
  // 2. valid Id / Type values and iterate rvs_pci_read_word_return_value
  test_cap[0]->id   = PCI_CAP_ID_EXP;
  test_cap[0]->type = PCI_CAP_NORMAL;
  test_cap[1]->id   = PCI_CAP_ID_EXP;
  test_cap[1]->type = PCI_CAP_NORMAL;
  get_num_bits(PCI_EXP_LNKSTA_NLW, &num_ones, &first_one, &max_value);
  for (int k = 0; k < max_value; k++) {
    rvs_pci_read_word_return_value = (k << first_one);
    buff = new char[1024];
    get_link_stat_neg_width(test_dev, buff);
    exp_string = "x" + std::to_string(k);
    EXPECT_STREQ(buff, exp_string.c_str());
  }

  // ---------------------------------------
  // get_slot_pwr_limit_value
  // ---------------------------------------
  // 1. invalid Id / Type (Id = PCI_CAP_ID_EXP and Type = PCI_CAP_NORMAL are valid)
  for (int id_f = 0; id_f < 2; id_f++) {
    for (int type_f = 0; type_f < 2; type_f++) {
      test_cap[id_f]->id           = 0;
      test_cap[type_f]->type       = 0;
      test_cap[(id_f+1)%2]->id     = PCI_CAP_ID_EXP;
      test_cap[(type_f+1)%2]->type = PCI_CAP_NORMAL;
      rvs_pci_read_long_return_value = 15;
      buff = new char[1024];
      get_slot_pwr_limit_value(test_dev, buff);

      if (id_f == type_f) {
        // other one is valid
        continue;
      } else {
        // none is valid
        EXPECT_STREQ(buff, "NOT SUPPORTED");
      }
    }
  }
  // 2. valid Id / Type values and iterate rvs_pci_read_long_return_value
  test_cap[0]->id   = PCI_CAP_ID_EXP;
  test_cap[0]->type = PCI_CAP_NORMAL;
  test_cap[1]->id   = PCI_CAP_ID_EXP;
  test_cap[1]->type = PCI_CAP_NORMAL;
  get_num_bits(PCI_EXP_SLTCAP_SPLV, &num_ones, &first_one, &max_value);
  for (int k = 0; k < max_value; k++) {
    rvs_pci_read_long_return_value = (k << first_one);
    buff = new char[1024];
    get_slot_pwr_limit_value(test_dev, buff);
    rvs_pci_read_long_return_value = k;
    if (rvs_pci_read_long_return_value > 0xEF) {
      switch (k) {
      case 0xF0:
          exp_string = "250.000W";
          break;
      case 0xF1:
          exp_string = "270.000W";
          break;
      case 0xF2:
          exp_string = "300.000W";
          break;
      default:
          exp_string = "-1.000W";
      }
    } else {
      exp_string = std::to_string(rvs_pci_read_long_return_value) + ".000W";
    }
    EXPECT_STREQ(buff, exp_string.c_str());
  }

  // ---------------------------------------
  // get_slot_physical_num
  // ---------------------------------------
  // 1. invalid Id / Type (Id = PCI_CAP_ID_EXP and Type = PCI_CAP_NORMAL are valid)
  for (int id_f = 0; id_f < 2; id_f++) {
    for (int type_f = 0; type_f < 2; type_f++) {
      test_cap[id_f]->id           = 0;
      test_cap[type_f]->type       = 0;
      test_cap[(id_f+1)%2]->id     = PCI_CAP_ID_EXP;
      test_cap[(type_f+1)%2]->type = PCI_CAP_NORMAL;
      rvs_pci_read_long_return_value = 15;
      buff = new char[1024];
      get_slot_physical_num(test_dev, buff);

      if (id_f == type_f) {
        // other one is valid
        continue;
      } else {
        // none is valid
        EXPECT_STREQ(buff, "NOT SUPPORTED");
      }
    }
  }
  // 2. valid Id / Type values and iterate rvs_pci_read_long_return_value
  test_cap[0]->id   = PCI_CAP_ID_EXP;
  test_cap[0]->type = PCI_CAP_NORMAL;
  test_cap[1]->id   = PCI_CAP_ID_EXP;
  test_cap[1]->type = PCI_CAP_NORMAL;
  get_num_bits(PCI_EXP_SLTCAP_PSN, &num_ones, &first_one, &max_value);
  for (int k = 0; k < max_value; k++) {
    rvs_pci_read_long_return_value = (k << first_one);
    buff = new char[1024];
    get_slot_physical_num(test_dev, buff);
    rvs_pci_read_long_return_value = k;
    exp_string = "#" + std::to_string(rvs_pci_read_long_return_value);
    EXPECT_STREQ(buff, exp_string.c_str());
  }

  // ---------------------------------------
  // get_pci_bus_id
  // ---------------------------------------
  get_pci_bus_id(test_dev, buff);
  exp_string = std::to_string(test_dev->bus);
  EXPECT_STREQ(buff, exp_string.c_str());

  // ---------------------------------------
  // get_device_id
  // ---------------------------------------
  get_device_id(test_dev, buff);
  exp_string = std::to_string(test_dev->device_id);
  EXPECT_STREQ(buff, exp_string.c_str());

  // ---------------------------------------
  // get_vendor_id
  // ---------------------------------------
  get_vendor_id(test_dev, buff);
  exp_string = std::to_string(test_dev->vendor_id);
  EXPECT_STREQ(buff, exp_string.c_str());

  for (int p = 0; p < 8; p++) {
    // ---------------------------------------
    // get_pci_bus_id
    // ---------------------------------------
    test_dev->bus = p;
    get_pci_bus_id(test_dev, buff);
    exp_string = std::to_string(test_dev->bus);
    EXPECT_STREQ(buff, exp_string.c_str());

    // ---------------------------------------
    // get_device_id
    // ---------------------------------------
    test_dev->device_id = (p + 1) % 8;
    get_device_id(test_dev, buff);
    exp_string = std::to_string(test_dev->device_id);
    EXPECT_STREQ(buff, exp_string.c_str());

    // ---------------------------------------
    // get_vendor_id
    // ---------------------------------------
    test_dev->vendor_id = (p + 2) % 8;
    get_vendor_id(test_dev, buff);
    exp_string = std::to_string(test_dev->vendor_id);
    EXPECT_STREQ(buff, exp_string.c_str());
  }

}


// 
// /**
//  * gets the PCI dev driver name
//  * @param dev a pci_dev structure containing the PCI device information
//  * @param buf pre-allocated char buffer
//  * @return 
//  */
// void get_kernel_driver(struct pci_dev *dev, char *buff) {
//     char name[1024], *drv, *base;
//     int n;
// 
//     buff[0] = '\0';
// 
//     if (dev->access->method != PCI_ACCESS_SYS_BUS_PCI) {
//         return;
//     }
// 
//     base = pci_get_param(dev->access, const_cast<char *>("sysfs.path"));
//     if (!base || !base[0]) {
//         return;
//     }
// 
//     n = snprintf(name, sizeof(name), "%s/devices/%04x:%02x:%02x.%d/driver",
//             base, dev->domain, dev->bus, dev->dev, dev->func);
//     if (n < 0 || n >= static_cast<int>(sizeof(name))) {
//         return;
//     }
// 
//     n = readlink(name, buff, PCI_CAP_DATA_MAX_BUF_SIZE);
//     if (n < 0) {
//         return;
//     }
// 
//     if (n >= PCI_CAP_DATA_MAX_BUF_SIZE) {
//         return;
//     }
// 
//     buff[n] = 0;
// 
//     if ((drv = strrchr(buff, '/')) != NULL)
//         snprintf(buff, PCI_CAP_DATA_MAX_BUF_SIZE, "%s", drv + 1);
// }
// 
// /**
//  * gets the device serial number
//  * @param dev a pci_dev structure containing the PCI device information
//  * @param buf pre-allocated char buffer
//  */
// void get_dev_serial_num(struct pci_dev *dev, char *buff) {
//     unsigned int cap_offset_dsn = pci_dev_find_cap_offset(dev,
//     PCI_EXT_CAP_ID_DSN, PCI_CAP_EXTENDED);
// 
//     if (cap_offset_dsn != 0) {
//         unsigned int t1, t2;
//         t1 = pci_read_long(dev, cap_offset_dsn + 4);
//         t2 = pci_read_long(dev, cap_offset_dsn + 8);
//         snprintf(buff, PCI_CAP_DATA_MAX_BUF_SIZE,
//                 "%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x", t2 >> 24,
//                 (t2 >> 16) & 0xff, (t2 >> 8) & 0xff, t2 & 0xff, t1 >> 24,
//                 (t1 >> 16) & 0xff, (t1 >> 8) & 0xff, t1 & 0xff);
//     } else {
//       snprintf(buff, PCI_CAP_DATA_MAX_BUF_SIZE, "%s", PCI_CAP_NOT_SUPPORTED);
//     }
// }
// 
// /**
//  * gets the device power budgeting capabilities
//  * @param dev a pci_dev structure containing the PCI device information
//  * @param pb_pm_state the PM State for the given operating condition
//  * @param pb_type the type of the given operating condition
//  * @param pb_power_rail thermal load or power rail for the given operating condition
//  * @param buf pre-allocated char buffer
//  */
// void get_pwr_budgeting(struct pci_dev *dev, uint8_t pb_pm_state,
//                        uint8_t pb_type, uint8_t pb_power_rail, char *buff) {
//     u16 i, w = 0;
//     u16 base, scale;
//     uint8_t pb_act_pm_state, pb_act_type, pb_act_power_rail;
// 
//     unsigned int cap_offset_pwbgd = pci_dev_find_cap_offset(dev,
//     PCI_EXT_CAP_ID_PWR, PCI_CAP_EXTENDED);
// 
//     snprintf(buff, PCI_CAP_DATA_MAX_BUF_SIZE, "%s", PCI_CAP_NOT_SUPPORTED);
// 
//     if (cap_offset_pwbgd != 0) {
//         i = 0;
// 
//         do {
//             pci_write_byte(dev, cap_offset_pwbgd + PCI_PWR_DSR, i);
//             w = pci_read_word(dev, cap_offset_pwbgd + PCI_PWR_DATA);
// 
//             if (!w)
//                 return;
// 
//             pb_act_pm_state = PCI_PWR_DATA_PM_STATE(w);
//             pb_act_type = PCI_PWR_DATA_TYPE(w);
//             pb_act_power_rail = PCI_PWR_DATA_RAIL(w);
// 
//             if (pb_act_pm_state == pb_pm_state && pb_act_type == pb_type &&
//                                         pb_act_power_rail == pb_power_rail) {
//                 base = PCI_PWR_DATA_BASE(w);
//                 scale = PCI_PWR_DATA_SCALE(w);
//                 snprintf(buff, PCI_CAP_DATA_MAX_BUF_SIZE, "%.3fW",
//                         base * pow(10, -scale));
//                 return;
//             }
// 
//             i++;
//         } while (1);
//     }
// }
// 
// /**
//  * Get current power state
//  * @param dev a pci_dev structure containing the PCI device information
//  * @param buf pre-allocated char buffer
//  */
// void get_pwr_curr_state(struct pci_dev *dev, char *buff) {
//   u16 pmcsr;
//   const char *type_s;
// 
//   // init output buffer with "not supported" message
//   snprintf(buff, PCI_CAP_DATA_MAX_BUF_SIZE, "%s", PCI_CAP_NOT_SUPPORTED);
// 
//   // fetch capability offset
//   unsigned int cap_offset = pci_dev_find_cap_offset(dev,
//     PCI_CAP_ID_PM, PCI_CAP_NORMAL);
// 
//   if (cap_offset == 0)
//     return;
// 
//   pmcsr = pci_read_word(dev, cap_offset + PCI_PM_CTRL);
// 
//   switch (pmcsr & PCI_PM_CTRL_STATE_MASK) {
//   case 0:
//       type_s = "D0";
//       break;
//   case 1:
//       type_s = "D1";
//       break;
//   case 2:
//       type_s = "D2";
//       break;
//   case 3:
//       type_s = "D3";
//       break;
//   }
// 
//   snprintf(buff, PCI_CAP_DATA_MAX_BUF_SIZE, "%s", type_s);
// }
// 
// /**
//  * gets the device atomic requester capabilities
//  * @param dev a pci_dev structure containing the PCI device information
//  * @param buf pre-allocated char buffer
//  */
// void get_atomic_op_routing(struct pci_dev *dev, char *buff) {
//     bool atomic_op_routing_enable = false;
// 
//     // get pci dev capabilities offset
//     unsigned int cap_offset = pci_dev_find_cap_offset(dev, PCI_CAP_ID_EXP,
//     PCI_CAP_NORMAL);
// 
//     if (cap_offset != 0) {
//         // get Capability version via
//         // PCI Express Capabilities Register (offset 02h)
//         u16 cap_flags = pci_read_word(dev, cap_offset + 2);
// 
//         // check if it's capability version 2
//         if (!((cap_flags & PCI_EXP_FLAGS_VERS) < 2)) {
//             u16 dev_ctl2_reg_val = pci_read_word(dev,
//                     cap_offset + PCI_EXP_DEVCTL2);
// 
//             // hardcoded 0x0040 because PCI_EXP_DEVCTL2_ATOMIC_REQ
//             // is not present on all versions of pci_regs.h
//             atomic_op_routing_enable = static_cast<bool>(dev_ctl2_reg_val
//                     & 0x0040);
// 
//             snprintf(buff, PCI_CAP_DATA_MAX_BUF_SIZE, "%s",
//                     atomic_op_routing_enable ? "TRUE" : "FALSE");
//         } else {
//           snprintf(buff, PCI_CAP_DATA_MAX_BUF_SIZE, "%s",
//               PCI_CAP_NOT_SUPPORTED);
//         }
//     } else {
//       snprintf(buff, PCI_CAP_DATA_MAX_BUF_SIZE, "%s", PCI_CAP_NOT_SUPPORTED);
//     }
// }
// 
// /**
//  * gets the device atomic capabilities register value
//  * @param dev a pci_dev structure containing the PCI device information
//  */
// int64_t get_atomic_op_register_value(struct pci_dev *dev) {
//     unsigned char i, has_memory_bar = 0;
// 
//     // get pci dev capabilities offset
//     unsigned int cap_offset = pci_dev_find_cap_offset(dev, PCI_CAP_ID_EXP,
//     PCI_CAP_NORMAL);
// 
//     if (cap_offset != 0) {
//         // get Capability version via
//         // PCI Express Capabilities Register (offset 02h)
//         u16 cap_flags = pci_read_word(dev, cap_offset + 2);
// 
//         // check if it's capability version 2
//         if (!((cap_flags & PCI_EXP_FLAGS_VERS) < 2)) {
//             // check if the device has memory space BAR
//             // (basically it should have but let us be sure about it)
//             for (i = 0; i < MEM_BAR_MAX_INDEX + 1; i++)
//                 if (dev->base_addr[i] && dev->size[i]) {
//                     if (!(dev->base_addr[i] & PCI_BASE_ADDRESS_SPACE_IO)) {
//                         has_memory_bar = 1;
//                         break;
//                     }
//                 }
// 
//             if (has_memory_bar) {
//                 return pci_read_long(dev, cap_offset + PCI_EXP_DEVCAP2);
// 
//             } else {
//               return -1;
//             }
//         } else {
//           return -1;
//         }
//     } else {
//       return -1;
//     }
// }
// 
// /**
//  * gets the device atomic 32-bit completer capabilities
//  * @param dev a pci_dev structure containing the PCI device information
//  * @param buf pre-allocated char buffer
//  */
// void get_atomic_op_32_completer(struct pci_dev *dev, char *buff) {
//   int64_t atomic_op_completer_value;
//   bool atomic_op_completer_supported_32_bit = false;
// 
//   atomic_op_completer_value = get_atomic_op_register_value(dev);
// 
//   if (atomic_op_completer_value != -1) {
//         atomic_op_completer_supported_32_bit =
//             static_cast<bool>(static_cast
//                 <unsigned int>(atomic_op_completer_value) & 0x0080);
// 
//         snprintf(buff, PCI_CAP_DATA_MAX_BUF_SIZE, "%s",
//                 atomic_op_completer_supported_32_bit ?
//                     "TRUE" : "FALSE");
//   } else {
//     snprintf(buff, PCI_CAP_DATA_MAX_BUF_SIZE, "%s", PCI_CAP_NOT_SUPPORTED);
//   }
// }
// 
// /**
//  * gets the device atomic 64-bit completer capabilities
//  * @param dev a pci_dev structure containing the PCI device information
//  * @param buf pre-allocated char buffer
//  */
// void get_atomic_op_64_completer(struct pci_dev *dev, char *buff) {
//   int64_t atomic_op_completer_value;
//   bool atomic_op_completer_supported_64_bit = false;
// 
//   atomic_op_completer_value = get_atomic_op_register_value(dev);
// 
//   if (atomic_op_completer_value != -1) {
//         atomic_op_completer_supported_64_bit =
//                 static_cast<bool>(static_cast
//                     <unsigned int>(atomic_op_completer_value) & 0x0100);
// 
//         snprintf(buff, PCI_CAP_DATA_MAX_BUF_SIZE, "%s",
//                 atomic_op_completer_supported_64_bit ?
//                     "TRUE" : "FALSE");
//   } else {
//     snprintf(buff, PCI_CAP_DATA_MAX_BUF_SIZE, "%s", PCI_CAP_NOT_SUPPORTED);
//   }
// }
// 
// /**
//  * gets the device atomic 128-bit CAS completer capabilities
//  * @param dev a pci_dev structure containing the PCI device information
//  * @param buf pre-allocated char buffer
//  */
// void get_atomic_op_128_CAS_completer(struct pci_dev *dev, char *buff) {
//   int64_t atomic_op_completer_value;
//   bool atomic_op_completer_supported_128_bit_CAS = false;
// 
//   atomic_op_completer_value = get_atomic_op_register_value(dev);
// 
//   if (atomic_op_completer_value != -1) {
//     atomic_op_completer_supported_128_bit_CAS =
//                 static_cast<bool>(static_cast
//                     <unsigned int>(atomic_op_completer_value) & 0x0200);
// 
//         snprintf(buff, PCI_CAP_DATA_MAX_BUF_SIZE, "%s",
//             atomic_op_completer_supported_128_bit_CAS ?
//                     "TRUE" : "FALSE");
//   } else {
//     snprintf(buff, PCI_CAP_DATA_MAX_BUF_SIZE, "%s", PCI_CAP_NOT_SUPPORTED);
//   }
// }
// 
// 
// // unsigned int pci_dev_find_cap_offset(struct pci_dev *dev, unsigned char cap);
// // void get_link_cap_max_speed(struct pci_dev *dev, char *buf);
// // void get_link_cap_max_width(struct pci_dev *dev, char *buff);
// // void get_link_stat_cur_speed(struct pci_dev *dev, char *buff);
// // void get_link_stat_neg_width(struct pci_dev *dev, char *buff);
// // void get_slot_pwr_limit_value(struct pci_dev *dev, char *buff);
// // void get_slot_physical_num(struct pci_dev *dev, char *buff);
// // void get_pci_bus_id(struct pci_dev *dev, char *buff);
// // void get_device_id(struct pci_dev *dev, char *buff);
// // void get_dev_serial_num(struct pci_dev *dev, char *buff);
// // void get_vendor_id(struct pci_dev *dev, char *buff);
// // void get_kernel_driver(struct pci_dev *dev, char *buff);
// // void get_pwr_budgeting(struct pci_dev *dev, uint8_t pb_pm_state,
// //                        uint8_t pb_type, uint8_t pb_power_rail, char *buff);
// // void get_pwr_curr_state(struct pci_dev *dev, char *buff);
// // void get_atomic_op_routing(struct pci_dev *dev, char *buff);
// // void get_atomic_op_32_completer(struct pci_dev *dev, char *buff);
// // void get_atomic_op_64_completer(struct pci_dev *dev, char *buff);
// // void get_atomic_op_128_CAS_completer(struct pci_dev *dev, char *buff);
// // int64_t get_atomic_op_register_value(struct pci_dev *dev);
// 
// */

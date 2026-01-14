/* Copyright (c) 2013-2021, Antmicro Ltd
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice, this
 *   list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <arch/zynq.h>
#include <arch/zynqmp.h>
#include <argp.h>
#include <bif.h>
#include <bootrom.h>
#include <common.h>

/* Prepare global variables for arg parser */
const char *argp_program_version = MKBOOTIMAGE_VER;
static char doc[] = "Generate bootloader images for Xilinx Zynq based platforms.";
static char args_doc[] = "[--parse-only|-p] [--zynqmp|-u] [--bit2bin|-b] [-i INPUT] [-o OUTPUT] [<input>] [<output>]";

static struct argp_option argp_options[] = {
  {"zynqmp", 'u', 0, 0, "Generate files for ZyqnMP (default is Zynq)", 0},
  {"parse-only", 'p', 0, 0, "Analyze BIF grammar, but don't generate any files", 0},
  {"bit2bin", 'b', 0, 0, "Treat input as bitstream and auto-generate BIF in memory", 0},
  {"input", 'i', "FILE", 0, "Input BIF/bit file (default: positional or derived)", 0},
  {"output", 'o', "FILE", 0, "Output bin file (default: derived from input)", 0},
  {0},
};

/* Prapare struct for holding parsed arguments */
struct arguments {
  bool zynqmp;
  bool parse_only;
  bool bit2bin;
  char *bif_filename;
  char *bin_filename;
};

static char *derive_filename(const char *src, const char *new_ext) {
  const char *sep = strrchr(src, '/');
  const char *sep_back = strrchr(src, '\\');
  const char *base = sep;
  size_t prefix_len;
  char *dot;
  char *out;

  if (!base || (sep_back && sep_back > base))
    base = sep_back;
  if (base)
    base++;
  else
    base = src;

  dot = strrchr(base, '.');
  if (dot)
    prefix_len = (size_t)(dot - src);
  else
    prefix_len = strlen(src);

  out = malloc(prefix_len + strlen(new_ext) + 1);
  if (!out)
    return NULL;
  memcpy(out, src, prefix_len);
  strcpy(out + prefix_len, new_ext);
  return out;
}

/* Define argument parser */
static error_t argp_parser(int key, char *arg, struct argp_state *state) {
  struct arguments *arguments = state->input;

  switch (key) {
  case 'u':
    arguments->zynqmp = true;
    break;
  case 'p':
    arguments->parse_only = true;
    break;
  case 'b':
    arguments->bit2bin = true;
    break;
  case 'i':
    arguments->bif_filename = arg;
    break;
  case 'o':
    arguments->bin_filename = arg;
    break;
  case ARGP_KEY_ARG:
    switch (state->arg_num) {
    case 0:
      if (arguments->bif_filename || arguments->bin_filename)
        argp_usage(state);
      arguments->bif_filename = arg;
      break;
    case 1:
      if (arguments->bin_filename)
        argp_usage(state);
      arguments->bin_filename = arg;
      break;
    default:
      argp_usage(state);
    }
    break;
  case ARGP_KEY_END:
    if (!arguments->bif_filename && !arguments->bin_filename)
      argp_usage(state);
    break;
  default:
    return ARGP_ERR_UNKNOWN;
  }
  return 0;
}

/* Finally initialize argp struct */
static struct argp argp = {argp_options, argp_parser, args_doc, doc, 0, 0, 0};

/* Declare the main function */
int main(int argc, char *argv[]) {
  FILE *ofile;
  uint32_t ofile_size;
  uint32_t *file_data;
  uint32_t esize, esize_aligned;
  struct arguments arguments;
  bootrom_ops_t *bops;
  bif_cfg_t cfg;
  error err;
  int i;
  char *derived_input = NULL;
  char *derived_output = NULL;
  int ret = EXIT_FAILURE;

  /* Init non-string arguments */
  memset(&arguments, 0, sizeof(arguments));

  /* Parse program arguments */
  argp_parse(&argp, argc, argv, 0, 0, &arguments);

  if (!arguments.bif_filename) {
    const char *ext = arguments.bit2bin ? ".bit" : ".bif";
    derived_input = derive_filename(arguments.bin_filename, ext);
    if (!derived_input) {
      ret = ERROR_NOMEM;
      goto cleanup;
    }
    arguments.bif_filename = derived_input;
  }

  if (!arguments.bin_filename && !arguments.parse_only) {
    derived_output = derive_filename(arguments.bif_filename, ".bin");
    if (!derived_output) {
      ret = ERROR_NOMEM;
      goto cleanup;
    }
    arguments.bin_filename = derived_output;
  }

  /* Print program version info */
  printf("%s\n", MKBOOTIMAGE_VER);

  init_bif_cfg(&cfg);

  /* Give bif parser the info about arch */
  cfg.arch = (arguments.zynqmp) ? BIF_ARCH_ZYNQMP : BIF_ARCH_ZYNQ;
  bops = (arguments.zynqmp) ? &zynqmp_bops : &zynq_bops;

  if (arguments.bit2bin) {
    const char *fmt = "all: { %s }\n";
    size_t bif_len = snprintf(NULL, 0, fmt, arguments.bif_filename) + 1;
    char *bif_buf = malloc(bif_len);
    if (!bif_buf)
      return ERROR_NOMEM;
    snprintf(bif_buf, bif_len, fmt, arguments.bif_filename);
    err = bif_parse_buf(bif_buf, strlen(bif_buf), "<bit2bin>", &cfg);
    free(bif_buf);
  } else {
    err = bif_parse(arguments.bif_filename, &cfg);
  }
  if (err) {
    ret = err;
    goto cleanup_cfg;
  }
  if (cfg.nodes_num == 0)
    goto cleanup_no_file;

  printf("Nodes found in the %s file:\n", arguments.bif_filename);
  for (i = 0; i < cfg.nodes_num; i++) {
    printf(" %s", cfg.nodes[i].fname);
    if (cfg.nodes[i].bootloader)
      printf(" (bootloader)\n");
    else
      printf("\n");
    if (cfg.nodes[i].load)
      printf("  load:   %08x\n", cfg.nodes[i].load);
    if (cfg.nodes[i].offset)
      printf("  offset: %08x\n", cfg.nodes[i].offset);
  }

  if (arguments.parse_only) {
    printf("The source BIF has a correct syntax\n");
    ret = EXIT_SUCCESS;
    goto cleanup_cfg;
  }

  /* Estimate memory required to fit all the binaries */
  esize = estimate_boot_image_size(&cfg);
  if (!esize)
    goto cleanup_no_file;

  /* Align estimated size to powers of two */
  esize_aligned = 2;
  while (esize_aligned < esize)
    esize_aligned *= 2;

  /* Allocate memory for output image */
  file_data = malloc(sizeof *file_data * esize_aligned);
  if (!file_data) {
    ret = ERROR_NOMEM;
    goto cleanup_cfg;
  }

  /* Generate bin file */
  err = create_boot_image(file_data, &cfg, bops, &ofile_size);
  if (err) {
    free(file_data);
    ret = err;
    goto cleanup_cfg;
  }

  ofile = fopen(arguments.bin_filename, "wb");
  if (ofile == NULL) {
    errorf("could not open output file: %s\n", arguments.bin_filename);
    ret = ERROR_CANT_WRITE;
    free(file_data);
    goto cleanup_cfg;
  }

  fwrite(file_data, sizeof(uint32_t), ofile_size, ofile);

  fclose(ofile);
  free(file_data);

  printf("All done, quitting\n");
  ret = EXIT_SUCCESS;

cleanup_cfg:
  deinit_bif_cfg(&cfg);
cleanup:
  free(derived_input);
  free(derived_output);
  return ret;

cleanup_no_file:
  ret = ERROR_BOOTROM_NOFILE;
  goto cleanup_cfg;
}

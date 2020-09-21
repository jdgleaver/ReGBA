#include <stdio.h>
#include <stdint.h>
#include <math.h>

/* opendingux devices are too slow to generate
 * a colour correction table at runtime (at least,
 * and rg350m takes 3-4 seconds to complete the
 * calculations, which is unacceptable). We therefore
 * have to pre-generate the lookup table array... */

/* Colour correction defines */
#define CC_TARGET_GAMMA   2.2f
#define CC_RGB_MAX       31.0f
#define CC_LUM            0.94f
#define CC_R              0.82f
#define CC_G              0.665f
#define CC_B              0.73f
#define CC_RG             0.125f
#define CC_RB             0.195f
#define CC_GR             0.24f
#define CC_GB             0.075f
#define CC_BR            -0.06f
#define CC_BG             0.21f
#define CC_GAMMA_ADJ      1.0f

#define CC_LUT_SIZE       32768

static uint16_t c_lut[CC_LUT_SIZE] = {0};

void init_lut(void)
{
   size_t color;
   float display_gamma_inv = 1.0f / CC_TARGET_GAMMA;
   float rgb_max_inv       = 1.0f / CC_RGB_MAX;
   float adjusted_gamma    = CC_TARGET_GAMMA + CC_GAMMA_ADJ;

   /* Populate colour correction look-up table */
   for (color = 0; color < CC_LUT_SIZE; color++)
   {
      unsigned r_final = 0;
      unsigned g_final = 0;
      unsigned b_final = 0;
      /* Extract values from BGR555 input */
      const unsigned r = color       & 0x1F;
      const unsigned g = color >>  5 & 0x1F;
      const unsigned b = color >> 10 & 0x1F;
      /* Perform gamma expansion */
      float r_float = pow((float)r * rgb_max_inv, adjusted_gamma);
      float g_float = pow((float)g * rgb_max_inv, adjusted_gamma);
      float b_float = pow((float)b * rgb_max_inv, adjusted_gamma);
      /* Perform colour mangling */
      float r_correct = CC_LUM * ((CC_R  * r_float) + (CC_GR * g_float) + (CC_BR * b_float));
      float g_correct = CC_LUM * ((CC_RG * r_float) + (CC_G  * g_float) + (CC_BG * b_float));
      float b_correct = CC_LUM * ((CC_RB * r_float) + (CC_GB * g_float) + (CC_B  * b_float));
      /* Range check... */
      r_correct = r_correct > 0.0f ? r_correct : 0.0f;
      g_correct = g_correct > 0.0f ? g_correct : 0.0f;
      b_correct = b_correct > 0.0f ? b_correct : 0.0f;
      /* Perform gamma compression */
      r_correct = pow(r_correct, display_gamma_inv);
      g_correct = pow(g_correct, display_gamma_inv);
      b_correct = pow(b_correct, display_gamma_inv);
      /* Range check... */
      r_correct = r_correct > 1.0f ? 1.0f : r_correct;
      g_correct = g_correct > 1.0f ? 1.0f : g_correct;
      b_correct = b_correct > 1.0f ? 1.0f : b_correct;
      /* Convert back to BGR555 */
      r_final = (unsigned)((r_correct * CC_RGB_MAX) + 0.5f) & 0x1F;
      g_final = (unsigned)((g_correct * CC_RGB_MAX) + 0.5f) & 0x1F;
      b_final = (unsigned)((b_correct * CC_RGB_MAX) + 0.5f) & 0x1F;
      c_lut[color] = 1 << 15 | b_final << 10 | g_final << 5 | r_final;
   }
}

int main ()
{
   FILE *file = NULL;
   size_t i;

   /* Populate lookup table */
   init_lut();

   /* Write header file */
   file = fopen("../cc_lut.h", "w");

   if (!file)
      return 1;

   fprintf(file,
         "#ifndef __CC_LUT_H__\n"
         "#define __CC_LUT_H__\n\n"
         "#include <stdint.h>\n\n"
         "extern const uint16_t CcLUT[];\n\n"
         "#endif /* __CC_LUT_H__ */\n");

   fclose(file);
   file = NULL;

   /* Write source file */
   file = fopen("../cc_lut.c", "w");

   if (!file)
      return 1;

   fprintf(file,
         "#include \"cc_lut.h\"\n\n"
         "const uint16_t CcLUT[] = {\n");

   for (i = 0; i < CC_LUT_SIZE; i++)
   {
      fprintf(file, " %#04x", c_lut[i]);

      if (i == CC_LUT_SIZE - 1)
         fprintf(file, "\n");
      else
      {
         if ((i + 1) % 5 == 0)
            fprintf(file, ",\n");
         else
            fprintf(file, ",");
      }
   }

   fprintf(file, "};\n");

   fclose(file);
   file = NULL;

   return 0;
}

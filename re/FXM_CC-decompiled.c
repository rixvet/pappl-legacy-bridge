/* FXM_CC (Xerox/Fuji 32-bit CUPS colour-correction filter) - Ghidra decompilation.
 * Binary: analysis/tarball/usr/lib/cups/filter/FXM_CC (ELF32 i386, with symbols).
 * Regenerate with re/DecompileFxmCc.java via analyzeHeadless (see RE-NOTES.md).
 * Interoperability research only; do not redistribute the original binary. */

//// ================= main @ 08048f30 =================

undefined4 main(int param_1,int param_2)

{
  undefined *puVar1;
  int iVar2;
  void *__ptr;
  int extraout_EAX;
  uint *puVar3;
  size_t sVar4;
  ssize_t sVar5;
  undefined4 uVar6;
  uint uVar7;
  char *pcVar8;
  int *piVar9;
  int iVar10;
  size_t __nbytes;
  uint *puVar11;
  undefined8 uVar12;
  FILE *pFVar13;
  undefined4 local_108;
  int local_104;
  uint local_fc;
  uint *local_f4;
  uint *local_ec;
  uint *local_e8;
  uint *local_e4;
  void *local_e0;
  void *local_d8;
  int local_d4;
  undefined1 local_cd;
  undefined4 local_cc;
  undefined4 local_c8;
  undefined4 local_c4;
  undefined4 local_c0;
  uint local_bc [4];
  uint local_ac;
  undefined4 local_98;
  uint local_8c;
  uint *local_88;
  uint local_80;
  int local_5c;
  int local_58;
  int local_54;
  int local_50;
  int local_4c;
  int local_48;
  int local_44;
  uint local_40;
  uint local_3c;
  undefined4 local_38;
  
  puVar1 = PTR_stderr_0805010c;
  local_c8 = 0;
  local_c0 = 0;
  local_e4 = (uint *)0x0;
  local_e8 = (uint *)0x0;
  local_c4 = 0;
  local_ec = (uint *)0x0;
  local_fc = 0xffffffff;
  local_cc = 0;
  local_108 = 0;
  setbuf(*(FILE **)PTR_stderr_0805010c,(char *)0x0);
  uVar7 = param_1 - 6;
  if (1 < uVar7) {
    pFVar13 = *(FILE **)puVar1;
    pcVar8 = "ERROR: fxcolorconv job-id user title copies options [file]\n";
    sVar4 = 0x3b;
LAB_0804908b:
    fwrite(pcVar8,1,sVar4,pFVar13);
    return 1;
  }
  local_d4 = 0;
  if ((param_1 != 6) && (local_d4 = open(*(char **)(param_2 + 0x18),0,uVar7,uVar7), local_d4 == -1))
  {
    pFVar13 = *(FILE **)puVar1;
    pcVar8 = "ERROR: fxcolorconv: unable to open raster file.\n";
    sVar4 = 0x30;
    goto LAB_0804908b;
  }
  uVar12 = CreatePPDSetting(&local_c8,&local_c4,&local_c0,*(undefined4 *)(param_2 + 0x14));
  uVar6 = (undefined4)((ulonglong)uVar12 >> 0x20);
  iVar2 = (int)uVar12;
  if ((iVar2 != 0) || (iVar2 = GetColorSetting(local_c8,&local_5c,uVar6,uVar6), iVar2 != 0)) {
    local_108 = 1;
    goto LAB_08049044;
  }
  local_d8 = malloc(0x30);
  __ptr = malloc(0x694);
  local_e0 = malloc(0x4fc4);
  if ((__ptr == (void *)0x0 || local_d8 == (void *)0x0) || (local_e0 == (void *)0x0)) {
    fwrite("ERROR: fxcolorconv: Memory not available.\n",1,0x2a,*(FILE **)PTR_stderr_0805010c);
    local_108 = 1;
  }
  else {
    iVar2 = local_58;
    if (((((local_58 == 0) && (local_54 == 0)) && (local_50 == 0)) &&
        ((local_4c == 0 && (local_48 == 0)))) && (iVar2 = local_44, local_44 == 0)) {
      local_d8 = (void *)0x0;
LAB_08049165:
      iVar2 = cil_Create_CST_Param_WpGm(__ptr,local_40 & 0xffff,local_3c & 0xffff,0);
      if (iVar2 == 0) {
        iVar2 = cil_Create_3Dlut_Param_RGB(local_38,local_e0,0,0);
        if (iVar2 != 0) {
          free(local_e0);
          local_e0 = (void *)0x0;
        }
        puVar3 = &local_8c;
        while (iVar2 = ReadFXRasterHeader(local_d4,&local_8c,puVar3,puVar3), iVar2 != 0) {
          puVar3 = &local_8c;
          puVar11 = local_bc;
          for (iVar10 = 0xb; iVar10 != 0; iVar10 = iVar10 + -1) {
            *puVar11 = *puVar3;
            puVar3 = puVar3 + 1;
            puVar11 = puVar11 + 1;
          }
          if (local_5c == 0x11) {
            local_bc[2] = 8;
            local_bc[3] = local_bc[3] / 3;
            local_98 = 0;
            iVar2 = local_ac * -0x55555555;
            local_ac = local_ac / 3;
          }
          puVar3 = (uint *)WriteFXRasterHeader(1,local_bc,iVar2,iVar2);
          if (puVar3 == (uint *)0x0) {
            piVar9 = __errno_location();
            fprintf(*(FILE **)PTR_stderr_0805010c,"ERROR: fxcolorconv: Write header failed. %d\n",
                    *piVar9);
LAB_08049624:
            local_108 = 1;
            break;
          }
          if (local_e4 == (uint *)0x0) {
            local_ec = (uint *)(0x100000 / (ulonglong)local_80);
            puVar3 = malloc(local_80 * (int)local_ec);
            local_e4 = puVar3;
            if (puVar3 != (uint *)0x0) goto LAB_080492c7;
LAB_08049696:
            pFVar13 = *(FILE **)PTR_stderr_0805010c;
            pcVar8 = "ERROR: fxcolorconv: Temporary memory not available.\n";
            sVar4 = 0x34;
LAB_0804961c:
            fwrite(pcVar8,1,sVar4,pFVar13);
            goto LAB_08049624;
          }
LAB_080492c7:
          puVar11 = local_88;
          if ((local_e8 == (uint *)0x0) &&
             (local_e8 = malloc((int)local_ec * local_80 + 1), puVar3 = local_e8, puVar11 = local_88
             , local_e8 == (uint *)0x0)) goto LAB_08049696;
          for (; 0 < (int)puVar11; puVar11 = (uint *)((int)puVar11 - (int)local_ec)) {
            puVar3 = puVar11;
            if ((int)local_ec < (int)puVar11) {
              puVar3 = local_ec;
            }
            sVar4 = (int)puVar3 * local_80;
            puVar3 = local_e4;
            for (__nbytes = sVar4; 0 < (int)__nbytes; __nbytes = __nbytes - sVar5) {
              sVar5 = read(local_d4,puVar3,__nbytes);
              if (sVar5 < 1) {
                pFVar13 = *(FILE **)PTR_stderr_0805010c;
                pcVar8 = "ERROR: fxcolorconv: File read failed.\n";
                sVar4 = 0x26;
                goto LAB_0804961c;
              }
              puVar3 = (uint *)((int)puVar3 + sVar5);
            }
            local_f4 = local_e8;
            local_104 = 0;
            puVar3 = local_e4;
            if (0 < (int)sVar4 / 3) {
              do {
                uVar7 = *puVar3 & 0xffffff;
                if (uVar7 == local_fc) {
                  if (local_5c != 0x16) goto LAB_08049498;
LAB_08049430:
                  *local_f4 = local_cc;
                  local_f4 = (uint *)((int)local_f4 + 3);
                }
                else {
                  iVar2 = (int)puVar3 + 1;
                  iVar10 = (int)puVar3 + 2;
                  if (__ptr != (void *)0x0) {
                    cil_CST_Conv_sBGR_Pixel(__ptr,puVar3,iVar2,iVar10,puVar3,iVar2,iVar10,uVar7);
                  }
                  uVar6 = 0;
                  if (local_d8 != (void *)0x0) {
                    uVar6 = cil_LCC_Conv_sBGR_Pixel
                                      (local_d8,puVar3,iVar2,iVar10,puVar3,iVar2,iVar10,local_d8);
                  }
                  local_fc = uVar7;
                  if (local_5c == 0x16) {
                    if (local_e0 == (void *)0x0) {
                      local_cc = CONCAT13(local_cc._3_1_,(int3)*puVar3);
                    }
                    else {
                      cil_3Dlut_Conv_sBGR_Pixel
                                (local_e0,puVar3,iVar2,iVar10,&local_cc,(int)&local_cc + 1,
                                 (int)&local_cc + 2,uVar6);
                    }
                    goto LAB_08049430;
                  }
                  cil_NTSC_Conv_Pixel(puVar3,iVar2,iVar10,&local_cd);
LAB_08049498:
                  *(undefined1 *)local_f4 = local_cd;
                  local_f4 = (uint *)((int)local_f4 + 1);
                }
                local_104 = local_104 + 1;
                puVar3 = (uint *)((int)puVar3 + 3);
              } while (local_104 < (int)sVar4 / 3);
            }
            puVar3 = local_e8;
            if (local_5c != 0x16) {
              sVar4 = (int)sVar4 / 3;
            }
            for (; 0 < (int)sVar4; sVar4 = sVar4 - sVar5) {
              sVar5 = write(1,puVar3,sVar4);
              if (sVar5 < 1) {
                pFVar13 = *(FILE **)PTR_stderr_0805010c;
                pcVar8 = "ERROR: fxcolorconv: File write failed.\n";
                sVar4 = 0x27;
                goto LAB_0804961c;
              }
              puVar3 = (uint *)((int)puVar3 + sVar5);
            }
            puVar3 = (uint *)((int)puVar11 - (int)local_ec);
          }
        }
        if (local_e4 != (uint *)0x0) {
          free(local_e4);
        }
        if (local_e8 != (uint *)0x0) {
          free(local_e8);
        }
        goto LAB_08049199;
      }
    }
    else {
      iVar2 = cil_Create_LCC_Param_sBGR
                        (local_d8,(int)(short)local_58,(int)(short)local_54,(int)(short)local_50,
                         (int)(short)local_4c,(int)(short)local_48,(int)(short)local_44,iVar2);
      if (iVar2 == 0) goto LAB_08049165;
    }
    local_108 = 1;
  }
LAB_08049199:
  if (local_d8 != (void *)0x0) {
    free(local_d8);
  }
  if (__ptr != (void *)0x0) {
    free(__ptr);
  }
  iVar2 = 0;
  if (local_e0 != (void *)0x0) {
    free(local_e0);
    iVar2 = extraout_EAX;
  }
LAB_08049044:
  ReleasePPDSetting(local_c8,local_c4,local_c0,iVar2);
  if (local_d4 != 0) {
    close(local_d4);
  }
  return local_108;
}



//// ================= cil_Create_LCC_Param_sBGR @ 08049730 =================

undefined4
cil_Create_LCC_Param_sBGR
          (int *param_1,short param_2,short param_3,short param_4,short param_5,short param_6,
          short param_7)

{
  double dVar1;
  double dVar2;
  double dVar3;
  double dVar4;
  double dVar5;
  double dVar6;
  double dVar7;
  double dVar8;
  double dVar9;
  short sVar10;
  undefined4 uVar11;
  short sVar12;
  short sVar13;
  
  dVar2 = DAT_0804eea0;
  uVar11 = 0xffffffff;
  if (param_1 != (int *)0x0) {
    sVar13 = -100;
    if ((-100 < param_4) && (sVar13 = param_4, 100 < param_4)) {
      sVar13 = 100;
    }
    sVar12 = -100;
    if ((-100 < param_2) && (sVar12 = param_2, 100 < param_2)) {
      sVar12 = 100;
    }
    sVar10 = -100;
    if ((-100 < param_3) && (sVar10 = param_3, 100 < param_3)) {
      sVar10 = 100;
    }
    dVar3 = (double)DAT_0804eec0;
    dVar1 = (double)sVar13 * DAT_0804ee70 + dVar3;
    dVar9 = (dVar3 - dVar1) * (double)DAT_0804eec4 + DAT_0804eeb8 * (double)sVar12;
    if (sVar10 < 0) {
      dVar8 = (double)sVar10 * DAT_0804ee78;
    }
    else {
      dVar8 = (double)sVar10 * DAT_0804ee80;
    }
    dVar8 = dVar8 + dVar3;
    dVar4 = dVar1 * DAT_0804ee88;
    dVar3 = (double)DAT_0804eec8;
    dVar7 = dVar1 * DAT_0804ee98;
    dVar5 = (double)DAT_0804eecc;
    dVar6 = dVar7 - DAT_0804ee98 * dVar8;
    dVar1 = dVar1 * DAT_0804eea0;
    *param_1 = (int)ROUND((dVar8 * DAT_0804ee90 + dVar4 + dVar3) * dVar5);
    dVar2 = dVar1 - dVar2 * dVar8;
    param_1[1] = (int)ROUND((dVar6 + dVar3) * dVar5);
    param_1[2] = (int)ROUND((dVar2 - dVar3) * dVar5);
    dVar4 = dVar4 - dVar8 * 0.3;
    param_1[3] = (int)ROUND((dVar4 - dVar3) * dVar5);
    param_1[4] = (int)ROUND(((dVar8 * DAT_0804eeb0 + dVar7) - dVar3) * dVar5);
    dVar8 = dVar8 * DAT_0804eea8;
    param_1[5] = (int)ROUND((dVar2 + dVar3) * dVar5);
    param_1[6] = (int)ROUND((dVar4 + dVar3) * dVar5);
    dVar2 = (double)param_5 * DAT_0804eeb8;
    param_1[7] = (int)ROUND((dVar6 - dVar3) * dVar5);
    param_1[8] = (int)ROUND(((dVar8 + dVar1) - dVar3) * dVar5);
    param_1[9] = (int)ROUND((dVar2 + dVar9) * dVar5);
    param_1[10] = (int)ROUND(((double)param_6 * DAT_0804eeb8 + dVar9) * dVar5);
    uVar11 = 0;
    param_1[0xb] = (int)ROUND(((double)param_7 * DAT_0804eeb8 + dVar9) * dVar5);
  }
  return uVar11;
}



//// ================= cil_Create_CST_Param_WpGm @ 080499c0 =================

undefined4 cil_Create_CST_Param_WpGm(int param_1,ushort param_2,undefined2 param_3)

{
  int iVar1;
  int iVar2;
  double dVar3;
  int iVar4;
  int iVar5;
  int iVar6;
  float fVar7;
  undefined *puVar8;
  undefined *puVar9;
  undefined *puVar10;
  int iVar11;
  int iVar12;
  int iVar13;
  int iVar14;
  short sVar15;
  double *pdVar16;
  int iVar17;
  int iVar18;
  int iVar19;
  int iVar20;
  short sVar21;
  int iVar22;
  int iVar23;
  int iVar24;
  int iVar25;
  int iVar26;
  double *local_1d4;
  undefined4 local_1c8;
  undefined4 local_1c4;
  undefined4 local_1c0;
  undefined4 local_1bc;
  undefined4 local_1b8;
  undefined4 local_1b4;
  undefined4 local_1b0;
  undefined4 local_1ac;
  undefined4 local_1a8;
  undefined4 local_1a4;
  undefined4 local_1a0;
  undefined4 local_19c;
  undefined4 local_198;
  undefined4 local_194;
  undefined4 local_190;
  undefined4 local_18c;
  short local_138;
  short local_136;
  double local_11c [10];
  undefined4 local_cc;
  undefined4 local_c8;
  undefined4 local_c4;
  undefined4 local_c0;
  undefined4 local_bc;
  undefined4 local_b8;
  undefined4 local_b4;
  undefined4 local_b0;
  undefined4 local_ac;
  undefined4 local_a8;
  undefined4 local_a4;
  undefined4 local_a0;
  undefined4 local_9c;
  undefined4 local_98;
  undefined4 local_94;
  undefined4 local_90;
  undefined4 local_8c;
  undefined4 local_88;
  undefined4 local_7c;
  undefined4 local_78;
  undefined4 local_74;
  undefined4 local_70;
  undefined4 local_6c;
  undefined4 local_68;
  undefined4 local_5c;
  undefined4 local_58;
  undefined4 local_54;
  undefined4 local_50;
  undefined4 local_4c;
  undefined4 local_48;
  undefined4 local_3c;
  undefined4 local_38;
  undefined4 local_34;
  undefined4 local_30;
  undefined4 local_2c;
  undefined4 local_28;
  
  puVar10 = PTR_gamma26TRC_08050128;
  puVar9 = PTR_gamma18TRC_08050118;
  puVar8 = PTR_gamma10TRC_08050108;
  if (param_1 == 0) {
    return 0xffffffff;
  }
  if (param_2 == 0x1964) {
    local_1d4 = *(double **)PTR_sRedXYZ_08050124;
    local_1c8 = *(undefined4 *)(PTR_sRedXYZ_08050124 + 4);
    local_1c4 = *(undefined4 *)(PTR_sRedXYZ_08050124 + 0x10);
    local_30 = *(undefined4 *)(PTR_sRedXYZ_08050124 + 0xc);
    local_34 = *(undefined4 *)(PTR_sRedXYZ_08050124 + 8);
    local_1c0 = *(undefined4 *)(PTR_sRedXYZ_08050124 + 0x14);
    local_1bc = *(undefined4 *)PTR_sGreenXYZ_08050110;
    local_1b8 = *(undefined4 *)(PTR_sGreenXYZ_08050110 + 4);
    local_1b4 = *(undefined4 *)(PTR_sGreenXYZ_08050110 + 8);
    local_1b0 = *(undefined4 *)(PTR_sGreenXYZ_08050110 + 0xc);
    local_1ac = *(undefined4 *)(PTR_sGreenXYZ_08050110 + 0x10);
    local_1a8 = *(undefined4 *)(PTR_sGreenXYZ_08050110 + 0x14);
    local_1a4 = *(undefined4 *)PTR_sBlueXYZ_08050100;
    local_1a0 = *(undefined4 *)(PTR_sBlueXYZ_08050100 + 4);
    local_19c = *(undefined4 *)(PTR_sBlueXYZ_08050100 + 8);
    local_198 = *(undefined4 *)(PTR_sBlueXYZ_08050100 + 0xc);
    local_194 = *(undefined4 *)(PTR_sBlueXYZ_08050100 + 0x10);
    local_190 = *(undefined4 *)(PTR_sBlueXYZ_08050100 + 0x14);
    local_18c = 0;
  }
  else {
    if (param_2 < 0x1965) {
      if (param_2 != 5000) goto _L59;
      local_1c4 = *(undefined4 *)(PTR_d50RedXYZ_08050104 + 0x10);
      local_1d4 = *(double **)PTR_d50RedXYZ_08050104;
      local_1c8 = *(undefined4 *)(PTR_d50RedXYZ_08050104 + 4);
      local_30 = *(undefined4 *)(PTR_d50RedXYZ_08050104 + 0xc);
      local_34 = *(undefined4 *)(PTR_d50RedXYZ_08050104 + 8);
      local_1c0 = *(undefined4 *)(PTR_d50RedXYZ_08050104 + 0x14);
      local_1bc = *(undefined4 *)PTR_d50GreenXYZ_080500fc;
      local_1b8 = *(undefined4 *)(PTR_d50GreenXYZ_080500fc + 4);
      local_1b4 = *(undefined4 *)(PTR_d50GreenXYZ_080500fc + 8);
      local_1b0 = *(undefined4 *)(PTR_d50GreenXYZ_080500fc + 0xc);
      local_1ac = *(undefined4 *)(PTR_d50GreenXYZ_080500fc + 0x10);
      local_1a8 = *(undefined4 *)(PTR_d50GreenXYZ_080500fc + 0x14);
      local_1a4 = *(undefined4 *)PTR_d50BlueXYZ_08050130;
      local_1a0 = *(undefined4 *)(PTR_d50BlueXYZ_08050130 + 4);
      local_19c = *(undefined4 *)(PTR_d50BlueXYZ_08050130 + 8);
      local_198 = *(undefined4 *)(PTR_d50BlueXYZ_08050130 + 0xc);
      local_194 = *(undefined4 *)(PTR_d50BlueXYZ_08050130 + 0x10);
      local_190 = *(undefined4 *)(PTR_d50BlueXYZ_08050130 + 0x14);
    }
    else {
      if (param_2 != 0x2454) goto _L59;
      local_1d4 = *(double **)PTR_k9300RedXYZ_0805011c;
      local_1c8 = *(undefined4 *)(PTR_k9300RedXYZ_0805011c + 4);
      local_1c4 = *(undefined4 *)(PTR_k9300RedXYZ_0805011c + 0x10);
      local_30 = *(undefined4 *)(PTR_k9300RedXYZ_0805011c + 0xc);
      local_34 = *(undefined4 *)(PTR_k9300RedXYZ_0805011c + 8);
      local_1c0 = *(undefined4 *)(PTR_k9300RedXYZ_0805011c + 0x14);
      local_1bc = *(undefined4 *)PTR_k9300GreenXYZ_08050134;
      local_1b8 = *(undefined4 *)(PTR_k9300GreenXYZ_08050134 + 4);
      local_1b4 = *(undefined4 *)(PTR_k9300GreenXYZ_08050134 + 8);
      local_1b0 = *(undefined4 *)(PTR_k9300GreenXYZ_08050134 + 0xc);
      local_1ac = *(undefined4 *)(PTR_k9300GreenXYZ_08050134 + 0x10);
      local_1a8 = *(undefined4 *)(PTR_k9300GreenXYZ_08050134 + 0x14);
      local_1a4 = *(undefined4 *)PTR_k9300BlueXYZ_08050114;
      local_1a0 = *(undefined4 *)(PTR_k9300BlueXYZ_08050114 + 4);
      local_19c = *(undefined4 *)(PTR_k9300BlueXYZ_08050114 + 8);
      local_198 = *(undefined4 *)(PTR_k9300BlueXYZ_08050114 + 0xc);
      local_194 = *(undefined4 *)(PTR_k9300BlueXYZ_08050114 + 0x10);
      local_190 = *(undefined4 *)(PTR_k9300BlueXYZ_08050114 + 0x14);
    }
    local_18c = 1;
  }
  switch(param_3) {
  case 10:
    local_3c = local_1d4;
    local_38 = local_1c8;
    local_2c = local_1c4;
    local_28 = local_1c0;
    local_5c = local_1bc;
    local_58 = local_1b8;
    local_54 = local_1b4;
    local_50 = local_1b0;
    local_4c = local_1ac;
    local_48 = local_1a8;
    local_7c = local_1a4;
    local_78 = local_1a0;
    local_74 = local_19c;
    local_70 = local_198;
    local_6c = local_194;
    local_68 = local_190;
    *(undefined4 *)(param_1 + 0x690) = local_18c;
    iVar22 = 0;
    sVar15 = 0xff;
    do {
      *(undefined2 *)(param_1 + iVar22 * 2) = *(undefined2 *)(puVar8 + iVar22 * 2);
      *(undefined2 *)(param_1 + 0x200 + iVar22 * 2) = *(undefined2 *)(puVar8 + iVar22 * 2);
      *(undefined2 *)(param_1 + 0x400 + iVar22 * 2) = *(undefined2 *)(puVar8 + iVar22 * 2);
      iVar22 = iVar22 + 1;
      sVar15 = sVar15 + -1;
    } while (-1 < sVar15);
    break;
  default:
_L59:
    *(undefined4 *)(param_1 + 0x62c) = 0xffffffff;
    *(undefined4 *)(param_1 + 0x690) = 0;
    return 0xffffffff;
  case 0xe:
    local_3c = local_1d4;
    local_38 = local_1c8;
    local_2c = local_1c4;
    local_28 = local_1c0;
    local_5c = local_1bc;
    local_58 = local_1b8;
    local_54 = local_1b4;
    local_50 = local_1b0;
    local_4c = local_1ac;
    local_48 = local_1a8;
    local_7c = local_1a4;
    local_78 = local_1a0;
    local_74 = local_19c;
    local_6c = local_194;
    local_70 = local_198;
    local_68 = local_190;
    iVar22 = 0;
    *(undefined4 *)(param_1 + 0x690) = local_18c;
    puVar8 = PTR_gamma14TRC_080500f8;
    sVar15 = 0xff;
    do {
      *(undefined2 *)(param_1 + iVar22 * 2) = *(undefined2 *)(puVar8 + iVar22 * 2);
      *(undefined2 *)(param_1 + 0x200 + iVar22 * 2) = *(undefined2 *)(puVar8 + iVar22 * 2);
      *(undefined2 *)(param_1 + 0x400 + iVar22 * 2) = *(undefined2 *)(puVar8 + iVar22 * 2);
      iVar22 = iVar22 + 1;
      sVar15 = sVar15 + -1;
    } while (-1 < sVar15);
    break;
  case 0x12:
    local_3c = local_1d4;
    local_38 = local_1c8;
    local_2c = local_1c4;
    local_28 = local_1c0;
    local_5c = local_1bc;
    local_58 = local_1b8;
    local_54 = local_1b4;
    local_50 = local_1b0;
    local_4c = local_1ac;
    local_48 = local_1a8;
    local_7c = local_1a4;
    local_78 = local_1a0;
    local_74 = local_19c;
    local_70 = local_198;
    local_6c = local_194;
    local_68 = local_190;
    *(undefined4 *)(param_1 + 0x690) = local_18c;
    iVar22 = 0;
    sVar15 = 0xff;
    do {
      *(undefined2 *)(param_1 + iVar22 * 2) = *(undefined2 *)(puVar9 + iVar22 * 2);
      *(undefined2 *)(param_1 + 0x200 + iVar22 * 2) = *(undefined2 *)(puVar9 + iVar22 * 2);
      *(undefined2 *)(param_1 + 0x400 + iVar22 * 2) = *(undefined2 *)(puVar9 + iVar22 * 2);
      iVar22 = iVar22 + 1;
      sVar15 = sVar15 + -1;
    } while (-1 < sVar15);
    break;
  case 0x16:
    local_3c = local_1d4;
    local_38 = local_1c8;
    local_2c = local_1c4;
    local_28 = local_1c0;
    local_5c = local_1bc;
    local_58 = local_1b8;
    local_54 = local_1b4;
    local_50 = local_1b0;
    local_4c = local_1ac;
    local_48 = local_1a8;
    local_7c = local_1a4;
    local_78 = local_1a0;
    local_74 = local_19c;
    local_6c = local_194;
    local_70 = local_198;
    local_68 = local_190;
    iVar22 = 0;
    *(undefined4 *)(param_1 + 0x690) = local_18c;
    puVar8 = PTR_gamma22TRC_08050120;
    sVar15 = 0xff;
    do {
      *(undefined2 *)(param_1 + iVar22 * 2) = *(undefined2 *)(puVar8 + iVar22 * 2);
      *(undefined2 *)(param_1 + 0x200 + iVar22 * 2) = *(undefined2 *)(puVar8 + iVar22 * 2);
      *(undefined2 *)(param_1 + 0x400 + iVar22 * 2) = *(undefined2 *)(puVar8 + iVar22 * 2);
      iVar22 = iVar22 + 1;
      sVar15 = sVar15 + -1;
    } while (-1 < sVar15);
    break;
  case 0x1a:
    local_3c = local_1d4;
    local_38 = local_1c8;
    local_2c = local_1c4;
    local_28 = local_1c0;
    local_5c = local_1bc;
    local_58 = local_1b8;
    local_54 = local_1b4;
    local_50 = local_1b0;
    local_4c = local_1ac;
    local_48 = local_1a8;
    local_7c = local_1a4;
    local_78 = local_1a0;
    local_74 = local_19c;
    local_70 = local_198;
    local_6c = local_194;
    local_68 = local_190;
    *(undefined4 *)(param_1 + 0x690) = local_18c;
    iVar22 = 0;
    sVar15 = 0xff;
    do {
      *(undefined2 *)(param_1 + iVar22 * 2) = *(undefined2 *)(puVar10 + iVar22 * 2);
      *(undefined2 *)(param_1 + 0x200 + iVar22 * 2) = *(undefined2 *)(puVar10 + iVar22 * 2);
      *(undefined2 *)(param_1 + 0x400 + iVar22 * 2) = *(undefined2 *)(puVar10 + iVar22 * 2);
      iVar22 = iVar22 + 1;
      sVar15 = sVar15 + -1;
    } while (-1 < sVar15);
  }
  fVar7 = DAT_0804eecc;
  local_cc = local_1d4;
  local_c8 = local_1c8;
  local_c4 = local_1bc;
  local_c0 = local_1b8;
  local_bc = local_1a4;
  local_b8 = local_1a0;
  local_b4 = local_34;
  local_b0 = local_30;
  local_ac = local_1b4;
  local_a8 = local_1b0;
  local_a4 = local_19c;
  local_a0 = local_198;
  local_9c = local_1c4;
  local_98 = local_1c0;
  local_94 = local_1ac;
  local_90 = local_1a8;
  local_8c = local_194;
  local_88 = local_190;
  local_136 = 0;
  do {
    local_138 = 0;
    do {
      iVar22 = local_136 * 3 + (int)local_138;
      local_11c[iVar22] = (double)DAT_0804eec8;
      pdVar16 = (double *)(&local_cc + local_138 * 2);
      local_1d4 = (double *)(PTR_XYZ2sRGBMatrix_0805012c + local_136 * 0x18);
      sVar15 = 2;
      do {
        dVar3 = *pdVar16;
        pdVar16 = pdVar16 + 3;
        local_11c[iVar22] = dVar3 * *local_1d4 + local_11c[iVar22];
        local_1d4 = local_1d4 + 1;
        sVar15 = sVar15 + -1;
      } while (-1 < sVar15);
      local_138 = local_138 + 1;
    } while (local_138 < 3);
    local_136 = local_136 + 1;
  } while (local_136 < 3);
  sVar15 = 0;
  do {
    sVar21 = 2;
    iVar22 = sVar15 * 3;
    do {
      *(int *)(param_1 + 0x600 + iVar22 * 4) = (int)ROUND((float)local_11c[iVar22] * fVar7);
      iVar22 = iVar22 + 1;
      sVar21 = sVar21 + -1;
    } while (-1 < sVar21);
    sVar15 = sVar15 + 1;
  } while (sVar15 < 3);
  if (*(int *)(param_1 + 0x690) != 0) {
    iVar26 = *(int *)(param_1 + 0x600) >> 2;
    iVar11 = *(int *)(param_1 + 0x604) >> 2;
    iVar23 = *(int *)(param_1 + 0x608) >> 2;
    iVar12 = iVar11 + iVar26;
    iVar17 = *(int *)(param_1 + 0x60c) >> 2;
    iVar24 = *(int *)(param_1 + 0x614) >> 2;
    *(int *)(param_1 + 0x630) = (iVar12 + iVar23) * 0xefff >> 0xe;
    iVar13 = *(int *)(param_1 + 0x610) >> 2;
    iVar18 = iVar17 + iVar13;
    iVar19 = *(int *)(param_1 + 0x618) >> 2;
    iVar25 = *(int *)(param_1 + 0x620) >> 2;
    *(int *)(param_1 + 0x634) = (iVar18 + iVar24) * 0xefff >> 0xe;
    iVar14 = *(int *)(param_1 + 0x61c) >> 2;
    iVar20 = iVar19 + iVar14;
    *(int *)(param_1 + 0x638) = (iVar20 + iVar25) * 0xefff >> 0xe;
    iVar22 = iVar11 * 0xefff + iVar26 * 0xffff;
    iVar4 = iVar23 * 0xefff;
    *(int *)(param_1 + 0x63c) = iVar22 + iVar4 >> 0xe;
    iVar1 = iVar13 * 0xefff + iVar17 * 0xffff;
    iVar5 = iVar24 * 0xefff;
    *(int *)(param_1 + 0x640) = iVar1 + iVar5 >> 0xe;
    iVar2 = iVar14 * 0xefff + iVar19 * 0xffff;
    iVar6 = iVar25 * 0xefff;
    *(int *)(param_1 + 0x644) = iVar2 + iVar6 >> 0xe;
    iVar11 = iVar11 * 0xffff + iVar26 * 0xefff;
    *(int *)(param_1 + 0x648) = iVar11 + iVar4 >> 0xe;
    iVar17 = iVar17 * 0xefff + iVar13 * 0xffff;
    *(int *)(param_1 + 0x64c) = iVar5 + iVar17 >> 0xe;
    iVar13 = iVar19 * 0xefff + iVar14 * 0xffff;
    *(int *)(param_1 + 0x650) = iVar6 + iVar13 >> 0xe;
    *(int *)(param_1 + 0x654) = iVar12 * 0xffff + iVar4 >> 0xe;
    *(int *)(param_1 + 0x658) = iVar18 * 0xffff + iVar5 >> 0xe;
    *(int *)(param_1 + 0x65c) = iVar20 * 0xffff + iVar6 >> 0xe;
    iVar23 = iVar23 * 0xffff;
    *(int *)(param_1 + 0x660) = iVar12 * 0xefff + iVar23 >> 0xe;
    iVar24 = iVar24 * 0xffff;
    *(int *)(param_1 + 0x664) = iVar18 * 0xefff + iVar24 >> 0xe;
    iVar25 = iVar25 * 0xffff;
    *(int *)(param_1 + 0x668) = iVar20 * 0xefff + iVar25 >> 0xe;
    *(undefined4 *)(param_1 + 0x684) = 0xffff;
    *(undefined4 *)(param_1 + 0x688) = 0xffff;
    *(int *)(param_1 + 0x66c) = iVar22 + iVar23 >> 0xe;
    *(undefined4 *)(param_1 + 0x68c) = 0xffff;
    *(int *)(param_1 + 0x670) = iVar1 + iVar24 >> 0xe;
    *(int *)(param_1 + 0x674) = iVar2 + iVar25 >> 0xe;
    *(int *)(param_1 + 0x678) = iVar11 + iVar23 >> 0xe;
    *(int *)(param_1 + 0x680) = iVar13 + iVar25 >> 0xe;
    *(int *)(param_1 + 0x67c) = iVar17 + iVar24 >> 0xe;
  }
  *(undefined4 *)(param_1 + 0x62c) = 0;
  return 0;
}



//// ================= cil_LCC_Conv_sBGR @ 0804a770 =================

void cil_LCC_Conv_sBGR(int *param_1,byte *param_2,byte *param_3,uint param_4,uint param_5,
                      int param_6)

{
  byte bVar1;
  byte bVar2;
  byte bVar3;
  int iVar4;
  uint uVar5;
  int iVar6;
  int iVar7;
  byte *pbVar8;
  uint uVar9;
  uint uVar10;
  uint local_34;
  int local_30;
  int local_2c;
  int local_28;
  uint local_1c;
  uint local_18;
  uint local_14;
  uint local_10;
  
  local_14 = 0xffffffff;
  local_18 = 0xffffffff;
  local_1c = 0xffffffff;
  local_28 = 0;
  local_2c = 0;
  local_30 = 0;
  local_34 = 0;
  if (param_3 != (byte *)0x0 && param_2 != (byte *)0x0) {
    if (param_1 == (int *)0x0) {
      local_10 = 0;
      if (param_4 != 0) {
        do {
          *param_3 = *param_2;
          pbVar8 = param_2 + 2;
          param_3[1] = param_2[1];
          param_2 = param_2 + 3;
          param_3[2] = *pbVar8;
          param_3 = param_3 + 3;
          uVar5 = local_34 + 1;
          local_34 = uVar5;
          if (param_5 <= uVar5) {
            local_34 = 0;
            iVar4 = param_6 + uVar5 * -3;
            param_2 = param_2 + iVar4;
            param_3 = param_3 + iVar4;
          }
          local_10 = local_10 + 1;
        } while (local_10 < param_4);
      }
    }
    else {
      local_10 = 0;
      if (param_4 != 0) {
        do {
          bVar1 = *param_2;
          bVar2 = param_2[1];
          bVar3 = param_2[2];
          param_2 = param_2 + 3;
          if ((bVar2 == local_18 && bVar1 == local_14) && (bVar3 == local_1c)) {
            *param_3 = (byte)local_28;
            param_3[1] = (byte)local_2c;
            param_3[2] = (byte)local_30;
          }
          else {
            uVar5 = (uint)CONCAT11(bVar1,bVar1);
            local_28 = 0;
            uVar9 = (uint)CONCAT11(bVar2,bVar2);
            uVar10 = (uint)CONCAT11(bVar3,bVar3);
            iVar6 = ((int)((*param_1 >> 2) * uVar5 + (param_1[1] >> 2) * uVar9 +
                          (param_1[2] >> 2) * uVar10) >> 0x16) +
                    (int)*(short *)((int)param_1 + 0x26);
            iVar7 = ((int)((param_1[3] >> 2) * uVar5 + (param_1[4] >> 2) * uVar9 +
                          (param_1[5] >> 2) * uVar10) >> 0x16) +
                    (int)*(short *)((int)param_1 + 0x2a);
            iVar4 = (int)*(short *)((int)param_1 + 0x2e) +
                    ((int)((param_1[6] >> 2) * uVar5 + (param_1[7] >> 2) * uVar9 +
                          (param_1[8] >> 2) * uVar10) >> 0x16);
            if ((0 < iVar6) && (local_28 = iVar6, 0xff < iVar6)) {
              local_28 = 0xff;
            }
            local_2c = 0;
            if ((0 < iVar7) && (local_2c = iVar7, 0xff < iVar7)) {
              local_2c = 0xff;
            }
            local_30 = 0;
            if ((0 < iVar4) && (local_30 = iVar4, 0xff < iVar4)) {
              local_30 = 0xff;
            }
            *param_3 = (byte)local_28;
            param_3[1] = (byte)local_2c;
            param_3[2] = (byte)local_30;
            local_14 = (uint)bVar1;
            local_18 = (uint)bVar2;
            local_1c = (uint)bVar3;
          }
          pbVar8 = param_3 + 3;
          uVar5 = local_34 + 1;
          if (param_5 <= local_34 + 1) {
            iVar4 = param_6 + (local_34 + 1) * -3;
            param_2 = param_2 + iVar4;
            local_34 = 0;
            pbVar8 = param_3 + iVar4 + 3;
            uVar5 = local_34;
          }
          local_34 = uVar5;
          param_3 = pbVar8;
          local_10 = local_10 + 1;
        } while (local_10 < param_4);
      }
    }
  }
  return;
}



//// ================= cil_LCC_Conv_sBGR_Pixel @ 0804aa60 =================

void cil_LCC_Conv_sBGR_Pixel
               (int *param_1,undefined1 *param_2,undefined1 *param_3,undefined1 *param_4,
               undefined1 *param_5,undefined1 *param_6,undefined1 *param_7)

{
  int iVar1;
  uint uVar2;
  int iVar3;
  int iVar4;
  int iVar5;
  uint uVar6;
  uint uVar7;
  
  if (param_1 != (int *)0x0) {
    uVar2 = (uint)CONCAT11(*param_2,*param_2);
    uVar6 = (uint)CONCAT11(*param_3,*param_3);
    uVar7 = (uint)CONCAT11(*param_4,*param_4);
    iVar3 = ((int)((*param_1 >> 2) * uVar2 + (param_1[1] >> 2) * uVar6 + (param_1[2] >> 2) * uVar7)
            >> 0x16) + (int)*(short *)((int)param_1 + 0x26);
    iVar4 = ((int)((param_1[3] >> 2) * uVar2 + (param_1[4] >> 2) * uVar6 + (param_1[5] >> 2) * uVar7
                  ) >> 0x16) + (int)*(short *)((int)param_1 + 0x2a);
    iVar5 = ((int)((param_1[6] >> 2) * uVar2 + (param_1[7] >> 2) * uVar6 + (param_1[8] >> 2) * uVar7
                  ) >> 0x16) + (int)*(short *)((int)param_1 + 0x2e);
    iVar1 = 0;
    if ((0 < iVar3) && (iVar1 = iVar3, 0xff < iVar3)) {
      iVar1 = 0xff;
    }
    *param_5 = (char)iVar1;
    iVar1 = 0;
    if ((0 < iVar4) && (iVar1 = iVar4, 0xff < iVar4)) {
      iVar1 = 0xff;
    }
    *param_6 = (char)iVar1;
    iVar1 = 0;
    if ((0 < iVar5) && (iVar1 = iVar5, 0xff < iVar5)) {
      iVar1 = 0xff;
    }
    *param_7 = (char)iVar1;
    return;
  }
  *param_5 = *param_2;
  *param_6 = *param_3;
  *param_7 = *param_4;
  return;
}



//// ================= cil_CST_Conv_sBGR @ 0804abc0 =================

void cil_CST_Conv_sBGR(int param_1,byte *param_2,byte *param_3,uint param_4,uint param_5,int param_6
                      )

{
  byte bVar1;
  byte bVar2;
  byte bVar3;
  undefined *puVar4;
  uint uVar5;
  uint uVar6;
  uint uVar7;
  uint uVar8;
  int iVar9;
  uint uVar10;
  uint uVar11;
  byte *pbVar12;
  int local_84;
  uint local_80;
  uint local_7c;
  int local_6c;
  int local_64;
  int local_60;
  int local_5c;
  uint local_40;
  uint local_3c;
  uint local_38;
  uint local_2c;
  uint local_28;
  uint local_24;
  uint local_18;
  uint local_14;
  
  puVar4 = PTR_XYZ2sRGBTrc_080500f4;
  local_18 = 0;
  if (param_3 != (byte *)0x0 && param_2 != (byte *)0x0) {
    if (param_1 == 0) {
      local_14 = 0;
      if (param_4 != 0) {
        do {
          *param_3 = *param_2;
          pbVar12 = param_2 + 2;
          param_3[1] = param_2[1];
          param_2 = param_2 + 3;
          param_3[2] = *pbVar12;
          param_3 = param_3 + 3;
          uVar6 = local_18 + 1;
          local_18 = uVar6;
          if (param_5 <= uVar6) {
            local_18 = 0;
            iVar9 = param_6 + uVar6 * -3;
            param_2 = param_2 + iVar9;
            param_3 = param_3 + iVar9;
          }
          local_14 = local_14 + 1;
        } while (local_14 < param_4);
      }
    }
    else if (*(int *)(param_1 + 0x62c) == 0) {
      local_14 = 0;
      local_24 = 0xffffffff;
      local_28 = 0xffffffff;
      local_2c = 0xffffffff;
      bVar1 = 0;
      bVar2 = 0;
      bVar3 = 0;
      if (param_4 != 0) {
        do {
          uVar6 = (uint)*param_2;
          uVar7 = (uint)param_2[1];
          uVar10 = (uint)param_2[2];
          param_2 = param_2 + 3;
          if ((uVar7 == local_28 && uVar6 == local_24) && (uVar10 == local_2c)) {
            *param_3 = bVar1;
            param_3[1] = bVar2;
            param_3[2] = bVar3;
          }
          else {
            uVar8 = (uint)*(ushort *)(param_1 + uVar6 * 2);
            uVar5 = (uint)*(ushort *)(param_1 + 0x200 + uVar7 * 2);
            uVar11 = (uint)*(ushort *)(param_1 + 0x400 + uVar10 * 2);
            if ((uVar8 < 0xf000 || *(int *)(param_1 + 0x690) == 0) ||
               (uVar11 < 0xf000 || uVar5 < 0xf000)) {
              local_38 = (int)((*(int *)(param_1 + 0x608) >> 2) * uVar11 +
                              (*(int *)(param_1 + 0x600) >> 2) * uVar8 +
                              (*(int *)(param_1 + 0x604) >> 2) * uVar5) >> 0x12;
              local_3c = (int)((*(int *)(param_1 + 0x614) >> 2) * uVar11 +
                              (*(int *)(param_1 + 0x60c) >> 2) * uVar8 +
                              (*(int *)(param_1 + 0x610) >> 2) * uVar5) >> 0x12;
              uVar8 = (int)((*(int *)(param_1 + 0x620) >> 2) * uVar11 +
                           (*(int *)(param_1 + 0x618) >> 2) * uVar8 +
                           (*(int *)(param_1 + 0x61c) >> 2) * uVar5) >> 0x12;
            }
            else {
              uVar8 = uVar8 & 0xfff;
              uVar5 = uVar5 & 0xfff;
              uVar11 = uVar11 & 0xfff;
              if (uVar8 < uVar5) {
                if (uVar11 < uVar5) {
                  if (uVar11 < uVar8) {
                    local_5c = 0x1000 - uVar5;
                    local_64 = uVar5 - uVar8;
                    iVar9 = uVar8 - uVar11;
                    local_38 = iVar9 * *(int *)(param_1 + 0x654) +
                               uVar11 * *(int *)(param_1 + 0x684) +
                               local_5c * *(int *)(param_1 + 0x630) +
                               local_64 * *(int *)(param_1 + 0x648);
                    local_3c = iVar9 * *(int *)(param_1 + 0x658) +
                               uVar11 * *(int *)(param_1 + 0x688) +
                               local_5c * *(int *)(param_1 + 0x634) +
                               local_64 * *(int *)(param_1 + 0x64c);
                    iVar9 = iVar9 * *(int *)(param_1 + 0x65c);
                    uVar8 = uVar11;
                  }
                  else {
                    local_5c = 0x1000 - uVar5;
                    local_64 = uVar5 - uVar11;
                    iVar9 = uVar11 - uVar8;
                    local_38 = iVar9 * *(int *)(param_1 + 0x678) + uVar8 * *(int *)(param_1 + 0x684)
                               + local_5c * *(int *)(param_1 + 0x630) +
                                 local_64 * *(int *)(param_1 + 0x648);
                    local_3c = iVar9 * *(int *)(param_1 + 0x67c) + uVar8 * *(int *)(param_1 + 0x688)
                               + local_5c * *(int *)(param_1 + 0x634) +
                                 local_64 * *(int *)(param_1 + 0x64c);
                    iVar9 = iVar9 * *(int *)(param_1 + 0x680);
                  }
                  local_38 = local_38 >> 0x10;
                  local_3c = local_3c >> 0x10;
                  iVar9 = iVar9 + uVar8 * *(int *)(param_1 + 0x68c);
                  local_84 = local_5c * *(int *)(param_1 + 0x638);
                  local_60 = local_64 * *(int *)(param_1 + 0x650);
                }
                else {
                  local_5c = 0x1000 - uVar11;
                  local_6c = uVar11 - uVar5;
                  iVar9 = uVar5 - uVar8;
                  local_38 = iVar9 * *(int *)(param_1 + 0x678) + uVar8 * *(int *)(param_1 + 0x684) +
                             local_5c * *(int *)(param_1 + 0x630) +
                             local_6c * *(int *)(param_1 + 0x660);
                  local_3c = iVar9 * *(int *)(param_1 + 0x67c) + uVar8 * *(int *)(param_1 + 0x688) +
                             local_5c * *(int *)(param_1 + 0x634) +
                             local_6c * *(int *)(param_1 + 0x664);
                  iVar9 = iVar9 * *(int *)(param_1 + 0x680);
LAB_0804b105:
                  local_38 = local_38 >> 0x10;
                  local_3c = local_3c >> 0x10;
                  iVar9 = iVar9 + uVar8 * *(int *)(param_1 + 0x68c);
                  local_84 = local_5c * *(int *)(param_1 + 0x638);
                  local_60 = local_6c * *(int *)(param_1 + 0x668);
                }
              }
              else {
                if (uVar5 < uVar11) {
                  if (uVar8 < uVar11) {
                    local_5c = 0x1000 - uVar11;
                    local_6c = uVar11 - uVar8;
                    iVar9 = uVar8 - uVar5;
                    local_38 = iVar9 * *(int *)(param_1 + 0x66c) + uVar5 * *(int *)(param_1 + 0x684)
                               + local_5c * *(int *)(param_1 + 0x630) +
                                 local_6c * *(int *)(param_1 + 0x660);
                    local_3c = iVar9 * *(int *)(param_1 + 0x670) + uVar5 * *(int *)(param_1 + 0x688)
                               + local_5c * *(int *)(param_1 + 0x634) +
                                 local_6c * *(int *)(param_1 + 0x664);
                    iVar9 = iVar9 * *(int *)(param_1 + 0x674);
                    uVar8 = uVar5;
                    goto LAB_0804b105;
                  }
                  local_5c = 0x1000 - uVar8;
                  local_60 = uVar8 - uVar11;
                  iVar9 = uVar11 - uVar5;
                  local_38 = iVar9 * *(int *)(param_1 + 0x66c) + uVar5 * *(int *)(param_1 + 0x684) +
                             local_5c * *(int *)(param_1 + 0x630) +
                             local_60 * *(int *)(param_1 + 0x63c);
                  local_3c = iVar9 * *(int *)(param_1 + 0x670) + uVar5 * *(int *)(param_1 + 0x688) +
                             local_5c * *(int *)(param_1 + 0x634) +
                             local_60 * *(int *)(param_1 + 0x640);
                  iVar9 = iVar9 * *(int *)(param_1 + 0x674);
                }
                else {
                  local_5c = 0x1000 - uVar8;
                  local_60 = uVar8 - uVar5;
                  iVar9 = uVar5 - uVar11;
                  local_38 = iVar9 * *(int *)(param_1 + 0x654) + uVar11 * *(int *)(param_1 + 0x684)
                             + local_5c * *(int *)(param_1 + 0x630) +
                               local_60 * *(int *)(param_1 + 0x63c);
                  local_3c = iVar9 * *(int *)(param_1 + 0x658) + uVar11 * *(int *)(param_1 + 0x688)
                             + local_5c * *(int *)(param_1 + 0x634) +
                               local_60 * *(int *)(param_1 + 0x640);
                  iVar9 = iVar9 * *(int *)(param_1 + 0x65c);
                  uVar5 = uVar11;
                }
                local_38 = local_38 >> 0x10;
                local_3c = local_3c >> 0x10;
                iVar9 = iVar9 + uVar5 * *(int *)(param_1 + 0x68c);
                local_84 = local_5c * *(int *)(param_1 + 0x638);
                local_60 = local_60 * *(int *)(param_1 + 0x644);
              }
              uVar8 = (uint)(local_84 + local_60 + iVar9) >> 0x10;
            }
            local_40 = 0;
            if ((0 < (int)local_38) && (local_40 = local_38, 0xfff < (int)local_38)) {
              local_40 = 0xfff;
            }
            local_7c = 0;
            if ((0 < (int)local_3c) && (local_7c = local_3c, 0xfff < (int)local_3c)) {
              local_7c = 0xfff;
            }
            local_80 = 0;
            if ((0 < (int)uVar8) && (local_80 = uVar8, 0xfff < (int)uVar8)) {
              local_80 = 0xfff;
            }
            bVar1 = puVar4[local_40];
            bVar2 = puVar4[local_7c];
            bVar3 = puVar4[local_80];
            *param_3 = bVar1;
            param_3[1] = bVar2;
            param_3[2] = bVar3;
            local_24 = uVar6;
            local_28 = uVar7;
            local_2c = uVar10;
          }
          pbVar12 = param_3 + 3;
          uVar6 = local_18 + 1;
          if (param_5 <= local_18 + 1) {
            iVar9 = param_6 + (local_18 + 1) * -3;
            param_2 = param_2 + iVar9;
            pbVar12 = param_3 + iVar9 + 3;
            local_18 = 0;
            uVar6 = local_18;
          }
          local_18 = uVar6;
          local_14 = local_14 + 1;
          param_3 = pbVar12;
        } while (local_14 < param_4);
      }
    }
    else {
      local_14 = 0;
      if (param_4 != 0) {
        do {
          *param_3 = *param_2;
          pbVar12 = param_2 + 2;
          param_3[1] = param_2[1];
          param_2 = param_2 + 3;
          param_3[2] = *pbVar12;
          param_3 = param_3 + 3;
          uVar6 = local_18 + 1;
          local_18 = uVar6;
          if (param_5 <= uVar6) {
            local_18 = 0;
            iVar9 = param_6 + uVar6 * -3;
            param_2 = param_2 + iVar9;
            param_3 = param_3 + iVar9;
          }
          local_14 = local_14 + 1;
        } while (local_14 < param_4);
      }
    }
  }
  return;
}



//// ================= cil_CST_Conv_sBGR_Pixel @ 0804b4b0 =================

void cil_CST_Conv_sBGR_Pixel
               (int param_1,byte *param_2,byte *param_3,byte *param_4,byte *param_5,byte *param_6,
               byte *param_7)

{
  undefined *puVar1;
  uint uVar2;
  int iVar3;
  uint uVar4;
  uint uVar5;
  uint uVar6;
  int local_40;
  int local_38;
  int local_34;
  int local_30;
  uint local_20;
  uint local_1c;
  
  puVar1 = PTR_XYZ2sRGBTrc_080500f4;
  if ((param_1 == 0) || (*(int *)(param_1 + 0x62c) != 0)) {
    *param_5 = *param_2;
    *param_6 = *param_3;
    *param_7 = *param_4;
    return;
  }
  uVar2 = (uint)*(ushort *)(param_1 + (uint)*param_2 * 2);
  uVar5 = (uint)*(ushort *)(param_1 + 0x200 + (uint)*param_3 * 2);
  uVar4 = (uint)*(ushort *)(param_1 + 0x400 + (uint)*param_4 * 2);
  if ((uVar2 < 0xf000 || *(int *)(param_1 + 0x690) == 0) || (uVar4 < 0xf000 || uVar5 < 0xf000)) {
    local_1c = (int)((*(int *)(param_1 + 0x608) >> 2) * uVar4 +
                    (*(int *)(param_1 + 0x600) >> 2) * uVar2 +
                    (*(int *)(param_1 + 0x604) >> 2) * uVar5) >> 0x12;
    uVar6 = (int)((*(int *)(param_1 + 0x614) >> 2) * uVar4 +
                 (*(int *)(param_1 + 0x60c) >> 2) * uVar2 + (*(int *)(param_1 + 0x610) >> 2) * uVar5
                 ) >> 0x12;
    uVar2 = (int)((*(int *)(param_1 + 0x620) >> 2) * uVar4 +
                 (*(int *)(param_1 + 0x618) >> 2) * uVar2 + (*(int *)(param_1 + 0x61c) >> 2) * uVar5
                 ) >> 0x12;
    goto LAB_0804b5f1;
  }
  uVar2 = uVar2 & 0xfff;
  uVar5 = uVar5 & 0xfff;
  uVar4 = uVar4 & 0xfff;
  if (uVar2 < uVar5) {
    if (uVar4 < uVar5) {
      if (uVar4 < uVar2) {
        local_30 = 0x1000 - uVar5;
        local_38 = uVar5 - uVar2;
        iVar3 = uVar2 - uVar4;
        local_1c = iVar3 * *(int *)(param_1 + 0x654) + uVar4 * *(int *)(param_1 + 0x684) +
                   local_30 * *(int *)(param_1 + 0x630) + local_38 * *(int *)(param_1 + 0x648);
        uVar6 = local_30 * *(int *)(param_1 + 0x634) + local_38 * *(int *)(param_1 + 0x64c) +
                iVar3 * *(int *)(param_1 + 0x658) + uVar4 * *(int *)(param_1 + 0x688);
        iVar3 = iVar3 * *(int *)(param_1 + 0x65c);
        uVar2 = uVar4;
      }
      else {
        local_30 = 0x1000 - uVar5;
        local_38 = uVar5 - uVar4;
        iVar3 = uVar4 - uVar2;
        local_1c = iVar3 * *(int *)(param_1 + 0x678) + uVar2 * *(int *)(param_1 + 0x684) +
                   local_30 * *(int *)(param_1 + 0x630) + local_38 * *(int *)(param_1 + 0x648);
        uVar6 = local_30 * *(int *)(param_1 + 0x634) + local_38 * *(int *)(param_1 + 0x64c) +
                iVar3 * *(int *)(param_1 + 0x67c) + uVar2 * *(int *)(param_1 + 0x688);
        iVar3 = iVar3 * *(int *)(param_1 + 0x680);
      }
      local_1c = local_1c >> 0x10;
      uVar6 = uVar6 >> 0x10;
      iVar3 = iVar3 + uVar2 * *(int *)(param_1 + 0x68c);
      local_30 = local_30 * *(int *)(param_1 + 0x638);
      local_34 = local_38 * *(int *)(param_1 + 0x650);
    }
    else {
      local_30 = 0x1000 - uVar4;
      local_40 = uVar4 - uVar5;
      iVar3 = uVar5 - uVar2;
      local_1c = iVar3 * *(int *)(param_1 + 0x678) + uVar2 * *(int *)(param_1 + 0x684) +
                 local_30 * *(int *)(param_1 + 0x630) + local_40 * *(int *)(param_1 + 0x660);
      uVar6 = local_30 * *(int *)(param_1 + 0x634) + local_40 * *(int *)(param_1 + 0x664) +
              iVar3 * *(int *)(param_1 + 0x67c) + uVar2 * *(int *)(param_1 + 0x688);
      iVar3 = iVar3 * *(int *)(param_1 + 0x680);
LAB_0804b815:
      local_1c = local_1c >> 0x10;
      uVar6 = uVar6 >> 0x10;
      iVar3 = iVar3 + uVar2 * *(int *)(param_1 + 0x68c);
      local_30 = local_30 * *(int *)(param_1 + 0x638);
      local_34 = local_40 * *(int *)(param_1 + 0x668);
    }
  }
  else {
    if (uVar5 < uVar4) {
      if (uVar2 < uVar4) {
        local_30 = 0x1000 - uVar4;
        local_40 = uVar4 - uVar2;
        iVar3 = uVar2 - uVar5;
        local_1c = iVar3 * *(int *)(param_1 + 0x66c) + uVar5 * *(int *)(param_1 + 0x684) +
                   local_30 * *(int *)(param_1 + 0x630) + local_40 * *(int *)(param_1 + 0x660);
        uVar6 = local_30 * *(int *)(param_1 + 0x634) + local_40 * *(int *)(param_1 + 0x664) +
                iVar3 * *(int *)(param_1 + 0x670) + uVar5 * *(int *)(param_1 + 0x688);
        iVar3 = iVar3 * *(int *)(param_1 + 0x674);
        uVar2 = uVar5;
        goto LAB_0804b815;
      }
      local_30 = 0x1000 - uVar2;
      local_34 = uVar2 - uVar4;
      iVar3 = uVar4 - uVar5;
      local_1c = iVar3 * *(int *)(param_1 + 0x66c) + uVar5 * *(int *)(param_1 + 0x684) +
                 local_30 * *(int *)(param_1 + 0x630) + local_34 * *(int *)(param_1 + 0x63c);
      uVar6 = local_30 * *(int *)(param_1 + 0x634) + local_34 * *(int *)(param_1 + 0x640) +
              iVar3 * *(int *)(param_1 + 0x670) + uVar5 * *(int *)(param_1 + 0x688);
      iVar3 = iVar3 * *(int *)(param_1 + 0x674);
    }
    else {
      local_30 = 0x1000 - uVar2;
      local_34 = uVar2 - uVar5;
      iVar3 = uVar5 - uVar4;
      local_1c = iVar3 * *(int *)(param_1 + 0x654) + uVar4 * *(int *)(param_1 + 0x684) +
                 local_30 * *(int *)(param_1 + 0x630) + local_34 * *(int *)(param_1 + 0x63c);
      uVar6 = local_30 * *(int *)(param_1 + 0x634) + local_34 * *(int *)(param_1 + 0x640) +
              iVar3 * *(int *)(param_1 + 0x658) + uVar4 * *(int *)(param_1 + 0x688);
      iVar3 = iVar3 * *(int *)(param_1 + 0x65c);
      uVar5 = uVar4;
    }
    local_1c = local_1c >> 0x10;
    uVar6 = uVar6 >> 0x10;
    iVar3 = iVar3 + uVar5 * *(int *)(param_1 + 0x68c);
    local_30 = local_30 * *(int *)(param_1 + 0x638);
    local_34 = local_34 * *(int *)(param_1 + 0x644);
  }
  uVar2 = (uint)(local_30 + local_34 + iVar3) >> 0x10;
LAB_0804b5f1:
  local_20 = 0;
  if ((0 < (int)local_1c) && (local_20 = local_1c, 0xfff < (int)local_1c)) {
    local_20 = 0xfff;
  }
  uVar5 = 0;
  if ((0 < (int)uVar6) && (uVar5 = uVar6, 0xfff < (int)uVar6)) {
    uVar5 = 0xfff;
  }
  uVar4 = 0;
  if ((0 < (int)uVar2) && (uVar4 = uVar2, 0xfff < (int)uVar2)) {
    uVar4 = 0xfff;
  }
  *param_5 = PTR_XYZ2sRGBTrc_080500f4[local_20];
  *param_6 = puVar1[uVar5];
  *param_7 = puVar1[uVar4];
  return;
}



//// ================= cil_NTSC_Conv @ 0804bb50 =================

/* WARNING: Removing unreachable block (ram,0x0804bbe6) */

undefined8 cil_NTSC_Conv(byte *param_1,undefined1 *param_2,uint param_3,uint param_4,int param_5)

{
  byte *pbVar1;
  byte *pbVar2;
  byte bVar3;
  uint uVar4;
  int iVar5;
  uint local_14;
  uint local_10;
  
  local_10 = 0;
  local_14 = 0;
  if (param_3 != 0) {
    do {
      bVar3 = *param_1;
      pbVar1 = param_1 + 1;
      pbVar2 = param_1 + 2;
      param_1 = param_1 + 3;
      *param_2 = (char)((uint)bVar3 * 0x4d + (uint)*pbVar1 * 0x97 + (uint)*pbVar2 * 0x1c >> 8);
      param_2 = param_2 + 1;
      uVar4 = local_14 + 1;
      local_14 = uVar4;
      if (param_4 <= uVar4) {
        local_14 = 0;
        iVar5 = param_5 + uVar4 * -3;
        param_1 = param_1 + iVar5;
        param_2 = param_2 + iVar5;
      }
      local_10 = local_10 + 1;
    } while (local_10 < param_3);
  }
  return CONCAT44(local_10,local_14);
}



//// ================= cil_NTSC_Conv_Pixel @ 0804bc00 =================

/* WARNING: Removing unreachable block (ram,0x0804bc50) */

void cil_NTSC_Conv_Pixel(byte *param_1,byte *param_2,byte *param_3,undefined1 *param_4)

{
  *param_4 = (char)((uint)*param_1 * 0x4d + (uint)*param_2 * 0x97 + (uint)*param_3 * 0x1c >> 8);
  return;
}



//// ================= CheckIfWhiteData @ 0804bc60 =================

undefined4 CheckIfWhiteData(char *param_1,uint param_2)

{
  uint uVar1;
  
  uVar1 = 0;
  if (param_2 != 0) {
    do {
      if (*param_1 != -1) {
        return 0;
      }
      uVar1 = uVar1 + 1;
      param_1 = param_1 + 1;
    } while (uVar1 < param_2);
  }
  return 1;
}



//// ================= ReadDTFInfo @ 0804bc90 =================

undefined4 ReadDTFInfo(int *param_1,int param_2)

{
  int iVar1;
  undefined4 uVar2;
  int iVar3;
  char *pcVar4;
  char *pcVar5;
  char *pcVar6;
  bool bVar7;
  int local_24;
  int local_14;
  
  local_14 = 0;
  *(undefined4 *)(param_2 + 0x24) = 0;
  *(undefined4 *)(param_2 + 0x28) = 0;
  *(undefined4 *)(param_2 + 0x2c) = 0;
  *(undefined4 *)(param_2 + 0x30) = 0;
  *(undefined4 *)(param_2 + 0x34) = 0;
  *(undefined4 *)(param_2 + 0x38) = 0;
  local_24 = *param_1;
  bVar7 = local_24 == 0;
  if (local_24 < 1) {
    return 0;
  }
LAB_0804bd80:
  do {
    iVar1 = param_1[1];
    iVar3 = 7;
    pcVar5 = *(char **)(iVar1 + local_14 * 8);
    pcVar4 = pcVar5;
    pcVar6 = "FXDLUT";
    do {
      if (iVar3 == 0) break;
      iVar3 = iVar3 + -1;
      bVar7 = *pcVar4 == *pcVar6;
      pcVar4 = pcVar4 + 1;
      pcVar6 = pcVar6 + 1;
    } while (bVar7);
    if (bVar7) {
      uVar2 = __strtol_internal(*(undefined4 *)(iVar1 + 4 + local_14 * 8),0,10,0);
      *(undefined4 *)(param_2 + 0x24) = uVar2;
LAB_0804bdc4:
      local_24 = *param_1;
      local_14 = local_14 + 1;
      bVar7 = local_14 == local_24;
      if (local_24 <= local_14) {
        return 0;
      }
      goto LAB_0804bd80;
    }
    iVar3 = 6;
    bVar7 = false;
    pcVar4 = pcVar5;
    pcVar6 = "FXUCR";
    do {
      if (iVar3 == 0) break;
      iVar3 = iVar3 + -1;
      bVar7 = *pcVar4 == *pcVar6;
      pcVar4 = pcVar4 + 1;
      pcVar6 = pcVar6 + 1;
    } while (bVar7);
    if (bVar7) {
      uVar2 = __strtol_internal(*(undefined4 *)(iVar1 + 4 + local_14 * 8),0,10,0);
      *(undefined4 *)(param_2 + 0x28) = uVar2;
      goto LAB_0804bdc4;
    }
    iVar3 = 6;
    bVar7 = false;
    pcVar4 = pcVar5;
    pcVar6 = "FXTRC";
    do {
      if (iVar3 == 0) break;
      iVar3 = iVar3 + -1;
      bVar7 = *pcVar4 == *pcVar6;
      pcVar4 = pcVar4 + 1;
      pcVar6 = pcVar6 + 1;
    } while (bVar7);
    if (bVar7) {
      uVar2 = __strtol_internal(*(undefined4 *)(iVar1 + 4 + local_14 * 8),0,10,0);
      *(undefined4 *)(param_2 + 0x2c) = uVar2;
      goto LAB_0804bdc4;
    }
    iVar3 = 9;
    bVar7 = false;
    pcVar4 = pcVar5;
    pcVar6 = "FXSCREEN";
    do {
      if (iVar3 == 0) break;
      iVar3 = iVar3 + -1;
      bVar7 = *pcVar4 == *pcVar6;
      pcVar4 = pcVar4 + 1;
      pcVar6 = pcVar6 + 1;
    } while (bVar7);
    if (bVar7) {
      uVar2 = __strtol_internal(*(undefined4 *)(iVar1 + 4 + local_14 * 8),0,10,0);
      *(undefined4 *)(param_2 + 0x30) = uVar2;
      goto LAB_0804bdc4;
    }
    iVar3 = 10;
    bVar7 = false;
    pcVar4 = pcVar5;
    pcVar6 = "FXMONOTRC";
    do {
      if (iVar3 == 0) break;
      iVar3 = iVar3 + -1;
      bVar7 = *pcVar4 == *pcVar6;
      pcVar4 = pcVar4 + 1;
      pcVar6 = pcVar6 + 1;
    } while (bVar7);
    if (bVar7) {
      uVar2 = __strtol_internal(*(undefined4 *)(iVar1 + 4 + local_14 * 8),0,10,0);
      *(undefined4 *)(param_2 + 0x34) = uVar2;
      goto LAB_0804bdc4;
    }
    iVar3 = 0xd;
    bVar7 = false;
    pcVar4 = "FXMONOSCREEN";
    do {
      if (iVar3 == 0) break;
      iVar3 = iVar3 + -1;
      bVar7 = *pcVar5 == *pcVar4;
      pcVar5 = pcVar5 + 1;
      pcVar4 = pcVar4 + 1;
    } while (bVar7);
    if (bVar7) {
      uVar2 = __strtol_internal(*(undefined4 *)(iVar1 + 4 + local_14 * 8),0,10,0);
      *(undefined4 *)(param_2 + 0x38) = uVar2;
      goto LAB_0804bdc4;
    }
    local_14 = local_14 + 1;
    bVar7 = local_14 == local_24;
    if (local_24 <= local_14) {
      return 0;
    }
  } while( true );
}



//// ================= GetDeviceColorParam @ 0804bea0 =================

undefined4 GetDeviceColorParam(undefined4 param_1,undefined4 param_2)

{
  int iVar1;
  undefined4 uVar2;
  int local_14 [2];
  
  local_14[0] = 0;
  local_14[1] = 0;
  iVar1 = FXParseMarkedCode(param_1,"FXDocumentType",local_14);
  uVar2 = 0xffffffff;
  if (iVar1 == 0) {
    iVar1 = ReadDTFInfo(local_14,param_2,0,0);
    uVar2 = 0xffffffff;
    if (iVar1 == 0) {
      if (0 < local_14[0]) {
        FXFreeMarkedCode(local_14);
      }
      uVar2 = 0;
    }
  }
  return uVar2;
}



//// ================= GetColorSetting @ 0804bf20 =================

uint GetColorSetting(undefined4 param_1,undefined4 *param_2)

{
  undefined4 uVar1;
  int iVar2;
  
  uVar1 = GetColorModeSetting(param_1);
  param_2[1] = 0;
  param_2[2] = 0;
  *param_2 = uVar1;
  param_2[3] = 0;
  param_2[4] = 0;
  param_2[5] = 0;
  param_2[6] = 0;
  param_2[7] = 0x1964;
  param_2[8] = 0x16;
  iVar2 = GetDeviceColorParam(param_1,param_2);
  return -(uint)(iVar2 == 0) ^ 0xffffffff;
}



//// ================= cil_3Dlut_Conv_sBGR @ 0804bfa0 =================

void cil_3Dlut_Conv_sBGR(int param_1,byte *param_2,undefined1 *param_3,uint param_4,uint param_5,
                        int param_6)

{
  byte bVar1;
  byte bVar2;
  byte bVar3;
  uint uVar4;
  uint uVar5;
  uint uVar6;
  uint uVar7;
  uint uVar8;
  uint uVar9;
  int iVar10;
  int iVar11;
  uint uVar12;
  uint uVar13;
  uint uVar14;
  uint local_70;
  uint local_6c;
  undefined1 local_67;
  undefined1 local_66;
  undefined1 local_65;
  int local_60;
  int local_5c;
  int local_58;
  int local_54;
  uint local_44;
  uint local_3c;
  uint local_38;
  uint local_34;
  uint local_28;
  uint local_24;
  uint local_20;
  uint local_1c;
  uint local_18;
  
  local_6c = 0;
  local_20 = 0xffffffff;
  local_24 = 0xffffffff;
  local_28 = 0xffffffff;
  local_65 = 0;
  local_66 = 0;
  local_67 = 0;
  local_70 = 0;
  if (param_4 != 0) {
    do {
      bVar1 = param_2[1];
      uVar13 = (uint)bVar1;
      bVar2 = *param_2;
      uVar7 = (uint)bVar2;
      bVar3 = param_2[2];
      uVar5 = (uint)bVar3;
      if ((bVar2 & bVar3 & bVar1) == 0xff) {
        *param_3 = 0xff;
        param_3[1] = 0xff;
        param_3[2] = 0xff;
      }
      else {
        if ((uVar13 != local_24 || uVar7 != local_20) || (uVar5 != local_28)) {
          uVar8 = uVar5 & 0xf;
          uVar9 = uVar7 & 0xf;
          uVar14 = uVar13 & 0xf;
          uVar4 = (uint)(bVar3 >> 4);
          uVar6 = (uint)(bVar1 >> 4);
          uVar12 = (uint)(bVar2 >> 4);
          if (uVar8 < uVar14) {
            if (uVar9 < uVar14) {
              if (uVar9 < uVar8) {
                local_54 = 0x10 - uVar14;
                local_5c = uVar14 - uVar8;
                iVar10 = uVar4 * 0x121 + uVar6 * 0x11 + uVar12;
                iVar11 = uVar8 - uVar9;
                uVar4 = *(uint *)(param_1 + 0x7c8 + iVar10 * 4);
                uVar6 = *(uint *)(param_1 + 0x7cc + iVar10 * 4);
                local_34 = *(uint *)(param_1 + 0x300 + iVar10 * 4);
                local_3c = *(uint *)(param_1 + 0x344 + iVar10 * 4);
                local_1c = *(byte *)(param_1 + 0x7ce + iVar10 * 4) * uVar9 +
                           (uint)*(byte *)(param_1 + 0x7ca + iVar10 * 4) * iVar11 +
                           (uint)*(byte *)(param_1 + 0x302 + iVar10 * 4) * local_54 +
                           (uint)*(byte *)(param_1 + 0x346 + iVar10 * 4) * local_5c;
                local_18 = (local_34 >> 8 & 0xff) * local_54 + (local_3c >> 8 & 0xff) * local_5c +
                           (uVar6 >> 8 & 0xff) * uVar9 + (uVar4 >> 8 & 0xff) * iVar11;
                iVar11 = (uVar4 & 0xff) * iVar11;
                iVar10 = (uVar6 & 0xff) * uVar9;
              }
              else {
                local_54 = 0x10 - uVar14;
                local_5c = uVar14 - uVar9;
                iVar10 = uVar4 * 0x121 + uVar6 * 0x11 + uVar12;
                iVar11 = uVar9 - uVar8;
                uVar4 = *(uint *)(param_1 + 0x348 + iVar10 * 4);
                uVar6 = *(uint *)(param_1 + 0x7cc + iVar10 * 4);
                local_34 = *(uint *)(param_1 + 0x300 + iVar10 * 4);
                local_3c = *(uint *)(param_1 + 0x344 + iVar10 * 4);
                local_1c = *(byte *)(param_1 + 0x7ce + iVar10 * 4) * uVar8 +
                           (uint)*(byte *)(param_1 + 0x34a + iVar10 * 4) * iVar11 +
                           (uint)*(byte *)(param_1 + 0x302 + iVar10 * 4) * local_54 +
                           (uint)*(byte *)(param_1 + 0x346 + iVar10 * 4) * local_5c;
                local_18 = (local_34 >> 8 & 0xff) * local_54 + (local_3c >> 8 & 0xff) * local_5c +
                           (uVar6 >> 8 & 0xff) * uVar8 + (uVar4 >> 8 & 0xff) * iVar11;
                iVar11 = (uVar4 & 0xff) * iVar11;
                iVar10 = (uVar6 & 0xff) * uVar8;
              }
              iVar11 = iVar11 + iVar10;
              local_60 = local_5c;
              local_44._0_1_ = (byte)local_3c;
            }
            else {
              iVar10 = uVar4 * 0x121 + uVar6 * 0x11 + uVar12;
              local_54 = 0x10 - uVar9;
              local_60 = uVar9 - uVar14;
              iVar11 = uVar14 - uVar8;
              uVar4 = *(uint *)(param_1 + 0x348 + iVar10 * 4);
              uVar6 = *(uint *)(param_1 + 0x7cc + iVar10 * 4);
              local_34 = *(uint *)(param_1 + 0x300 + iVar10 * 4);
              local_44 = *(uint *)(param_1 + 0x304 + iVar10 * 4);
              local_1c = *(byte *)(param_1 + 0x7ce + iVar10 * 4) * uVar8 +
                         (uint)*(byte *)(param_1 + 0x34a + iVar10 * 4) * iVar11 +
                         (uint)*(byte *)(param_1 + 0x302 + iVar10 * 4) * local_54 +
                         (uint)*(byte *)(param_1 + 0x306 + iVar10 * 4) * local_60;
              local_18 = (local_34 >> 8 & 0xff) * local_54 + (local_44 >> 8 & 0xff) * local_60 +
                         (uVar6 >> 8 & 0xff) * uVar8 + (uVar4 >> 8 & 0xff) * iVar11;
              iVar11 = (uVar4 & 0xff) * iVar11;
              iVar10 = (uVar6 & 0xff) * uVar8;
LAB_0804c365:
              iVar11 = iVar11 + iVar10;
            }
            local_54 = (uint)(byte)local_34 * local_54;
            local_58 = (uint)(byte)local_44 * local_60;
          }
          else {
            if (uVar14 < uVar9) {
              if (uVar8 < uVar9) {
                iVar10 = uVar4 * 0x121 + uVar6 * 0x11 + uVar12;
                local_54 = 0x10 - uVar9;
                local_60 = uVar9 - uVar8;
                uVar4 = *(uint *)(param_1 + 0x788 + iVar10 * 4);
                iVar11 = uVar8 - uVar14;
                uVar6 = *(uint *)(param_1 + 0x7cc + iVar10 * 4);
                local_34 = *(uint *)(param_1 + 0x300 + iVar10 * 4);
                local_44 = *(uint *)(param_1 + 0x304 + iVar10 * 4);
                local_1c = *(byte *)(param_1 + 0x7ce + iVar10 * 4) * uVar14 +
                           (uint)*(byte *)(param_1 + 0x78a + iVar10 * 4) * iVar11 +
                           (uint)*(byte *)(param_1 + 0x302 + iVar10 * 4) * local_54 +
                           (uint)*(byte *)(param_1 + 0x306 + iVar10 * 4) * local_60;
                local_18 = (local_34 >> 8 & 0xff) * local_54 + (local_44 >> 8 & 0xff) * local_60 +
                           (uVar6 >> 8 & 0xff) * uVar14 + (uVar4 >> 8 & 0xff) * iVar11;
                iVar11 = (uVar4 & 0xff) * iVar11;
                iVar10 = (uVar6 & 0xff) * uVar14;
                goto LAB_0804c365;
              }
              iVar10 = uVar4 * 0x121 + uVar6 * 0x11 + uVar12;
              local_54 = 0x10 - uVar8;
              local_58 = uVar8 - uVar9;
              uVar4 = *(uint *)(param_1 + 0x788 + iVar10 * 4);
              iVar11 = uVar9 - uVar14;
              uVar6 = *(uint *)(param_1 + 0x7cc + iVar10 * 4);
              local_34 = *(uint *)(param_1 + 0x300 + iVar10 * 4);
              local_38 = *(uint *)(param_1 + 0x784 + iVar10 * 4);
              local_1c = *(byte *)(param_1 + 0x7ce + iVar10 * 4) * uVar14 +
                         (uint)*(byte *)(param_1 + 0x78a + iVar10 * 4) * iVar11 +
                         (uint)*(byte *)(param_1 + 0x302 + iVar10 * 4) * local_54 +
                         (uint)*(byte *)(param_1 + 0x786 + iVar10 * 4) * local_58;
              local_18 = (local_34 >> 8 & 0xff) * local_54 + (local_38 >> 8 & 0xff) * local_58 +
                         (uVar6 >> 8 & 0xff) * uVar14 + (uVar4 >> 8 & 0xff) * iVar11;
              iVar11 = (uVar4 & 0xff) * iVar11;
              iVar10 = (uVar6 & 0xff) * uVar14;
            }
            else {
              iVar10 = uVar4 * 0x121 + uVar6 * 0x11 + uVar12;
              local_54 = 0x10 - uVar8;
              local_58 = uVar8 - uVar14;
              iVar11 = uVar14 - uVar9;
              uVar4 = *(uint *)(param_1 + 0x7c8 + iVar10 * 4);
              uVar6 = *(uint *)(param_1 + 0x7cc + iVar10 * 4);
              local_34 = *(uint *)(param_1 + 0x300 + iVar10 * 4);
              local_38 = *(uint *)(param_1 + 0x784 + iVar10 * 4);
              local_1c = *(byte *)(param_1 + 0x7ce + iVar10 * 4) * uVar9 +
                         (uint)*(byte *)(param_1 + 0x7ca + iVar10 * 4) * iVar11 +
                         (uint)*(byte *)(param_1 + 0x302 + iVar10 * 4) * local_54 +
                         (uint)*(byte *)(param_1 + 0x786 + iVar10 * 4) * local_58;
              local_18 = (local_34 >> 8 & 0xff) * local_54 + (local_38 >> 8 & 0xff) * local_58 +
                         (uVar6 >> 8 & 0xff) * uVar9 + (uVar4 >> 8 & 0xff) * iVar11;
              iVar11 = (uVar4 & 0xff) * iVar11;
              iVar10 = (uVar6 & 0xff) * uVar9;
            }
            iVar11 = iVar11 + iVar10;
            local_54 = (local_34 & 0xff) * local_54;
            local_58 = (local_38 & 0xff) * local_58;
          }
          local_18 = local_18 >> 4;
          local_1c = local_1c >> 4;
          local_65 = 0;
          uVar4 = (uint)(iVar11 + local_54 + local_58) >> 4;
          if (local_1c != 0) {
            if (0xff < local_1c) {
              local_1c = 0xff;
            }
            local_65 = (undefined1)local_1c;
          }
          local_66 = 0;
          if (local_18 != 0) {
            if (0xff < local_18) {
              local_18 = 0xff;
            }
            local_66 = (undefined1)local_18;
          }
          local_67 = 0;
          local_28 = uVar5;
          local_24 = uVar13;
          local_20 = uVar7;
          if (uVar4 != 0) {
            if (0xff < uVar4) {
              uVar4 = 0xff;
            }
            local_67 = (undefined1)uVar4;
          }
        }
        *param_3 = local_65;
        param_3[1] = local_66;
        param_3[2] = local_67;
      }
      param_3 = param_3 + 3;
      param_2 = param_2 + 3;
      uVar13 = local_70 + 1;
      local_70 = uVar13;
      if (param_5 <= uVar13) {
        local_70 = 0;
        iVar11 = param_6 + uVar13 * -3;
        param_2 = param_2 + iVar11;
        param_3 = param_3 + iVar11;
      }
      local_6c = local_6c + 1;
    } while (local_6c < param_4);
  }
  return;
}



//// ================= cil_3Dlut_Conv_sBGR_Pixel @ 0804c7d0 =================

void cil_3Dlut_Conv_sBGR_Pixel
               (int param_1,byte *param_2,byte *param_3,byte *param_4,undefined1 *param_5,
               undefined1 *param_6,undefined1 *param_7)

{
  byte bVar1;
  byte bVar2;
  byte bVar3;
  uint uVar4;
  uint uVar5;
  int iVar6;
  undefined1 uVar7;
  uint uVar8;
  int iVar9;
  uint uVar10;
  uint uVar11;
  uint uVar12;
  undefined4 local_5c;
  undefined4 local_54;
  undefined4 local_50;
  undefined4 local_4c;
  undefined4 local_3c;
  undefined4 local_34;
  undefined4 local_30;
  undefined4 local_1c;
  undefined4 local_18;
  
  bVar1 = *param_2;
  bVar2 = *param_3;
  bVar3 = *param_4;
  if ((bVar1 & bVar3 & bVar2) == 0xff) {
    *param_5 = 0xff;
    *param_6 = 0xff;
    *param_7 = 0xff;
    return;
  }
  uVar11 = (uint)(bVar2 >> 4);
  uVar8 = bVar3 & 0xf;
  uVar12 = (uint)(bVar1 >> 4);
  uVar5 = bVar2 & 0xf;
  uVar10 = bVar1 & 0xf;
  uVar4 = (uint)(bVar3 >> 4);
  if (uVar8 < uVar5) {
    if (uVar10 < uVar5) {
      if (uVar10 < uVar8) {
        iVar9 = uVar4 * 0x121 + uVar11 * 0x11 + uVar12;
        local_4c = 0x10 - uVar5;
        iVar6 = uVar8 - uVar10;
        local_54 = uVar5 - uVar8;
        uVar5 = *(uint *)(param_1 + 0x7c8 + iVar9 * 4);
        uVar8 = *(uint *)(param_1 + 0x7cc + iVar9 * 4);
        uVar4 = *(uint *)(param_1 + 0x300 + iVar9 * 4);
        local_34 = *(uint *)(param_1 + 0x344 + iVar9 * 4);
        local_1c = *(byte *)(param_1 + 0x7ce + iVar9 * 4) * uVar10 +
                   (uint)*(byte *)(param_1 + 0x7ca + iVar9 * 4) * iVar6 +
                   (uint)*(byte *)(param_1 + 0x302 + iVar9 * 4) * local_4c +
                   (uint)*(byte *)(param_1 + 0x346 + iVar9 * 4) * local_54;
        local_18 = (uVar4 >> 8 & 0xff) * local_4c + (local_34 >> 8 & 0xff) * local_54 +
                   (uVar8 >> 8 & 0xff) * uVar10 + (uVar5 >> 8 & 0xff) * iVar6;
        iVar6 = (uVar5 & 0xff) * iVar6;
        iVar9 = (uVar8 & 0xff) * uVar10;
      }
      else {
        iVar9 = uVar4 * 0x121 + uVar11 * 0x11 + uVar12;
        local_4c = 0x10 - uVar5;
        local_54 = uVar5 - uVar10;
        iVar6 = uVar10 - uVar8;
        uVar5 = *(uint *)(param_1 + 0x348 + iVar9 * 4);
        uVar10 = *(uint *)(param_1 + 0x7cc + iVar9 * 4);
        uVar4 = *(uint *)(param_1 + 0x300 + iVar9 * 4);
        local_34 = *(uint *)(param_1 + 0x344 + iVar9 * 4);
        local_1c = *(byte *)(param_1 + 0x7ce + iVar9 * 4) * uVar8 +
                   (uint)*(byte *)(param_1 + 0x34a + iVar9 * 4) * iVar6 +
                   (uint)*(byte *)(param_1 + 0x302 + iVar9 * 4) * local_4c +
                   (uint)*(byte *)(param_1 + 0x346 + iVar9 * 4) * local_54;
        local_18 = (uVar4 >> 8 & 0xff) * local_4c + (local_34 >> 8 & 0xff) * local_54 +
                   (uVar10 >> 8 & 0xff) * uVar8 + (uVar5 >> 8 & 0xff) * iVar6;
        iVar9 = (uVar10 & 0xff) * uVar8;
        iVar6 = (uVar5 & 0xff) * iVar6;
      }
      iVar6 = iVar6 + iVar9;
      local_4c = (uVar4 & 0xff) * local_4c;
      local_50 = (local_34 & 0xff) * local_54;
      goto LAB_0804c95c;
    }
    iVar9 = uVar4 * 0x121 + uVar11 * 0x11 + uVar12;
    local_4c = 0x10 - uVar10;
    local_5c = uVar10 - uVar5;
    iVar6 = uVar5 - uVar8;
    uVar4 = *(uint *)(param_1 + 0x348 + iVar9 * 4);
    uVar5 = *(uint *)(param_1 + 0x7cc + iVar9 * 4);
    uVar10 = *(uint *)(param_1 + 0x300 + iVar9 * 4);
    local_3c = *(uint *)(param_1 + 0x304 + iVar9 * 4);
    local_1c = *(byte *)(param_1 + 0x7ce + iVar9 * 4) * uVar8 +
               (uint)*(byte *)(param_1 + 0x34a + iVar9 * 4) * iVar6 +
               (uint)*(byte *)(param_1 + 0x302 + iVar9 * 4) * local_4c +
               (uint)*(byte *)(param_1 + 0x306 + iVar9 * 4) * local_5c;
    local_18 = (uVar10 >> 8 & 0xff) * local_4c + (local_3c >> 8 & 0xff) * local_5c +
               (uVar5 >> 8 & 0xff) * uVar8 + (uVar4 >> 8 & 0xff) * iVar6;
    iVar6 = (uVar4 & 0xff) * iVar6;
    iVar9 = (uVar5 & 0xff) * uVar8;
LAB_0804cadd:
    iVar6 = iVar6 + iVar9;
    local_4c = (uVar10 & 0xff) * local_4c;
    local_50 = local_5c;
    local_30._0_1_ = (byte)local_3c;
  }
  else {
    if (uVar5 < uVar10) {
      if (uVar8 < uVar10) {
        iVar9 = uVar4 * 0x121 + uVar11 * 0x11 + uVar12;
        local_4c = 0x10 - uVar10;
        local_5c = uVar10 - uVar8;
        iVar6 = uVar8 - uVar5;
        uVar4 = *(uint *)(param_1 + 0x788 + iVar9 * 4);
        uVar8 = *(uint *)(param_1 + 0x7cc + iVar9 * 4);
        uVar10 = *(uint *)(param_1 + 0x300 + iVar9 * 4);
        local_3c = *(uint *)(param_1 + 0x304 + iVar9 * 4);
        local_1c = *(byte *)(param_1 + 0x7ce + iVar9 * 4) * uVar5 +
                   (uint)*(byte *)(param_1 + 0x78a + iVar9 * 4) * iVar6 +
                   (uint)*(byte *)(param_1 + 0x302 + iVar9 * 4) * local_4c +
                   (uint)*(byte *)(param_1 + 0x306 + iVar9 * 4) * local_5c;
        local_18 = (uVar10 >> 8 & 0xff) * local_4c + (local_3c >> 8 & 0xff) * local_5c +
                   (uVar8 >> 8 & 0xff) * uVar5 + (uVar4 >> 8 & 0xff) * iVar6;
        iVar6 = (uVar4 & 0xff) * iVar6;
        iVar9 = (uVar8 & 0xff) * uVar5;
        goto LAB_0804cadd;
      }
      iVar9 = uVar4 * 0x121 + uVar11 * 0x11 + uVar12;
      local_4c = 0x10 - uVar8;
      local_50 = uVar8 - uVar10;
      iVar6 = uVar10 - uVar5;
      uVar8 = *(uint *)(param_1 + 0x788 + iVar9 * 4);
      uVar10 = *(uint *)(param_1 + 0x7cc + iVar9 * 4);
      uVar4 = *(uint *)(param_1 + 0x300 + iVar9 * 4);
      local_30 = *(uint *)(param_1 + 0x784 + iVar9 * 4);
      local_1c = *(byte *)(param_1 + 0x7ce + iVar9 * 4) * uVar5 +
                 (uint)*(byte *)(param_1 + 0x78a + iVar9 * 4) * iVar6 +
                 (uint)*(byte *)(param_1 + 0x302 + iVar9 * 4) * local_4c +
                 (uint)*(byte *)(param_1 + 0x786 + iVar9 * 4) * local_50;
      local_18 = (uVar4 >> 8 & 0xff) * local_4c + (local_30 >> 8 & 0xff) * local_50 +
                 (uVar10 >> 8 & 0xff) * uVar5 + (uVar8 >> 8 & 0xff) * iVar6;
      iVar9 = (uVar10 & 0xff) * uVar5;
      iVar6 = (uVar8 & 0xff) * iVar6;
    }
    else {
      iVar9 = uVar4 * 0x121 + uVar11 * 0x11 + uVar12;
      local_4c = 0x10 - uVar8;
      local_50 = uVar8 - uVar5;
      iVar6 = uVar5 - uVar10;
      uVar5 = *(uint *)(param_1 + 0x7c8 + iVar9 * 4);
      uVar8 = *(uint *)(param_1 + 0x7cc + iVar9 * 4);
      uVar4 = *(uint *)(param_1 + 0x300 + iVar9 * 4);
      local_30 = *(uint *)(param_1 + 0x784 + iVar9 * 4);
      local_1c = *(byte *)(param_1 + 0x7ce + iVar9 * 4) * uVar10 +
                 (uint)*(byte *)(param_1 + 0x7ca + iVar9 * 4) * iVar6 +
                 (uint)*(byte *)(param_1 + 0x302 + iVar9 * 4) * local_4c +
                 (uint)*(byte *)(param_1 + 0x786 + iVar9 * 4) * local_50;
      local_18 = (uVar4 >> 8 & 0xff) * local_4c + (local_30 >> 8 & 0xff) * local_50 +
                 (uVar8 >> 8 & 0xff) * uVar10 + (uVar5 >> 8 & 0xff) * iVar6;
      iVar6 = (uVar5 & 0xff) * iVar6;
      iVar9 = (uVar8 & 0xff) * uVar10;
    }
    iVar6 = iVar6 + iVar9;
    local_4c = (uVar4 & 0xff) * local_4c;
  }
  local_50 = (uint)(byte)local_30 * local_50;
LAB_0804c95c:
  local_18 = local_18 >> 4;
  local_1c = local_1c >> 4;
  uVar4 = (uint)(iVar6 + local_4c + local_50) >> 4;
  uVar7 = 0;
  if (local_1c != 0) {
    if (0xff < local_1c) {
      local_1c = 0xff;
    }
    uVar7 = (undefined1)local_1c;
  }
  *param_5 = uVar7;
  uVar7 = 0;
  if (local_18 != 0) {
    if (0xff < local_18) {
      local_18 = 0xff;
    }
    uVar7 = (undefined1)local_18;
  }
  *param_6 = uVar7;
  uVar7 = 0;
  if (uVar4 != 0) {
    if (0xff < uVar4) {
      uVar4 = 0xff;
    }
    uVar7 = (undefined1)uVar4;
  }
  *param_7 = uVar7;
  return;
}



//// ================= cil_Create_3Dlut_Param_RGB @ 0804cfc0 =================

/* WARNING: Removing unreachable block (ram,0x0804d37a) */
/* WARNING: Restarted to delay deadcode elimination for space: stack */

undefined4 cil_Create_3Dlut_Param_RGB(int *param_1,void *param_2)

{
  char *pcVar1;
  int iVar2;
  FILE *__stream;
  size_t sVar3;
  char *pcVar4;
  int *piVar5;
  void *pvVar6;
  int *extraout_ECX;
  int *extraout_ECX_00;
  int *piVar7;
  int *extraout_ECX_01;
  int *extraout_ECX_02;
  int *extraout_ECX_03;
  int *extraout_ECX_04;
  int *extraout_ECX_05;
  int *extraout_ECX_06;
  int iVar8;
  char *pcVar9;
  int local_240;
  undefined4 local_234;
  int local_22c;
  void *local_228;
  int local_224;
  void *local_220;
  char local_21c [524];
  
  pcVar4 = "FXDlutFilePath";
  local_234 = 0;
  pcVar9 = local_21c;
  pcVar1 = getenv("PPD");
  iVar2 = FXGetPPDAttribute(pcVar1,pcVar4,pcVar9);
  if (iVar2 != 0) {
    fxdebugstr(3,"DEBUG: DLUT file path not specified in PPD.\n",local_21c,local_21c);
    return 0xffffffff;
  }
  piVar5 = &local_22c;
  __stream = fopen(local_21c,"r");
  if (__stream == (FILE *)0x0) {
    piVar5 = __errno_location();
    fxdebugstr(3,"DEBUG: OpenDlutFile: DLUT file %s cannot open %d.\n",local_21c,*piVar5);
    piVar7 = extraout_ECX_02;
LAB_0804d1f2:
    __stream = (FILE *)0x0;
  }
  else {
    local_240 = 0;
    local_228 = (void *)0x0;
    local_220 = (void *)0x0;
    fseek(__stream,4,0);
    sVar3 = fread(piVar5,1,4,__stream);
    piVar7 = extraout_ECX;
    iVar2 = -1;
    if (sVar3 == 4) {
      SwapLong(piVar5);
      pvVar6 = malloc(local_22c << 2);
      piVar7 = extraout_ECX_03;
      iVar2 = -1;
      local_228 = pvVar6;
      if (pvVar6 != (void *)0x0) {
        iVar2 = 0;
        if (0 < local_22c) {
          do {
            sVar3 = fread(pvVar6,1,4,__stream);
            if (sVar3 != 4) {
              iVar2 = ferror(__stream);
              fxdebugstr(3,"DEBUG: ParseDlutFile: Data read failed %d.\n",iVar2);
              goto LAB_0804d2c1;
            }
            iVar2 = iVar2 + 1;
            SwapLong(pvVar6);
            pvVar6 = (void *)((int)pvVar6 + 4);
          } while (iVar2 < local_22c);
        }
        sVar3 = fread(&local_224,1,4,__stream);
        if (sVar3 == 4) {
          SwapLong(&local_224);
          pvVar6 = malloc(local_224 << 2);
          local_220 = pvVar6;
          if (pvVar6 != (void *)0x0) {
            iVar8 = 0;
            piVar7 = piVar5;
            iVar2 = local_240;
            if (0 < local_224) {
              do {
                sVar3 = fread(pvVar6,1,4,__stream);
                if (sVar3 != 4) goto LAB_0804d2c1;
                iVar8 = iVar8 + 1;
                SwapLong(pvVar6);
                pvVar6 = (void *)((int)pvVar6 + 4);
                piVar7 = extraout_ECX_04;
              } while (iVar8 < local_224);
            }
            goto LAB_0804d093;
          }
        }
LAB_0804d2c1:
        local_240 = -1;
        if (local_228 != (void *)0x0) {
          free(local_228);
          piVar5 = extraout_ECX_06;
        }
        piVar7 = piVar5;
        iVar2 = local_240;
        if (local_220 != (void *)0x0) {
          free(local_220);
          piVar7 = extraout_ECX_05;
        }
      }
    }
LAB_0804d093:
    if (iVar2 != 0) {
      fxdebugstr(3,"DEBUG: OpenDlutFile: DLUT file parse failed.\n",__stream,__stream);
      fclose(__stream);
      piVar7 = extraout_ECX_01;
      goto LAB_0804d1f2;
    }
  }
  if (__stream == (FILE *)0x0) {
    pcVar4 = "DEBUG: DLUT file cannot open.\n";
    piVar7 = (int *)0x0;
  }
  else {
    if (((int)param_1 <= local_224 + -1) &&
       (iVar2 = *(int *)((int)local_220 + (int)param_1 * 4), piVar7 = param_1, iVar2 <= local_22c))
    {
      fseek(__stream,*(int *)((int)local_228 + iVar2 * 4 + -4) + 8,0);
      sVar3 = fread(param_2,1,0x4fc4,__stream);
      piVar7 = extraout_ECX_00;
      if (sVar3 == 0x4fc4) {
        iVar8 = 0x1330;
        iVar2 = (int)param_2 + 0x300;
        do {
          SwapUlong(iVar2);
          iVar8 = iVar8 + -1;
          iVar2 = iVar2 + 4;
        } while (-1 < iVar8);
        goto LAB_0804d157;
      }
    }
    pcVar4 = "DEBUG: DLUT retrieve failed.\n";
  }
  fxdebugstr(3,pcVar4,piVar7,piVar7);
  local_234 = 0xffffffff;
LAB_0804d157:
  if (__stream != (FILE *)0x0) {
    if (local_228 != (void *)0x0) {
      free(local_228);
    }
    if (local_220 != (void *)0x0) {
      free(local_220);
    }
    fclose(__stream);
  }
  return local_234;
}




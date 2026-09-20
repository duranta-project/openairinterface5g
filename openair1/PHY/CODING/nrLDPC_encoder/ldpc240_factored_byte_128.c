#include <string.h>
#include "PHY/sse_intrin.h"
// generated code for BG1 Zc=240, byte encoding, factored
// 319 terms vs 2105 expanded (6.60x); pipeline lag 1 chunk(s)
static inline void ldpc240_byte_128(uint8_t *c,uint8_t *d) {
  simde__m128i *csimd=(simde__m128i *)c,*dsimd=(simde__m128i *)d;
  simde__m128i *c2,*d2,*cm,*dm;
  uint8_t kd[1920] __attribute__((aligned(64)));
  int i2;

  // stage 1: P0, the kernel-free extension rows, and the kernel rows
  // pipelined 1 chunk(s) behind P0 so no loop split is needed
  for (i2=0; i2<15; i2++) {
     c2=&csimd[i2];
     d2=&dsimd[i2];

//row: 0
     d2[0]=simde_mm_xor_si128(c2[6],simde_mm_xor_si128(c2[8],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[9],c2[8],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[12],c2[11],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[31],c2[30],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[32],c2[31],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[45],c2[44],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[68],c2[67],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[75],c2[74],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[75],c2[74],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[99],c2[98],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[99],c2[98],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[101],c2[100],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[130],c2[129],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[134],c2[133],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[134],c2[133],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[156],c2[155],4),simde_mm_xor_si128(c2[158],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[165],c2[164],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[183],c2[182],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[186],c2[185],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[188],c2[187],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[216],c2[215],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[222],c2[221],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[222],c2[221],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[246],c2[245],12),simde_mm_xor_si128(c2[246],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[251],c2[250],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[272],c2[271],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[274],c2[273],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[274],c2[273],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[301],c2[300],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[305],c2[304],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[315],c2[314],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[331],c2[330],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[343],c2[342],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[344],c2[343],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[362],c2[361],6),simde_mm_xor_si128(c2[368],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[372],c2[371],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[391],c2[390],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[392],c2[391],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[395],c2[394],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[426],c2[425],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[427],c2[426],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[431],c2[430],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[453],c2[452],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[456],c2[455],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[459],c2[458],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[490],c2[489],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[493],c2[492],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[494],c2[493],9),simde_mm_xor_si128(c2[512],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[516],c2[515],7),simde_mm_xor_si128(c2[516],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[548],c2[547],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[554],c2[553],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[555],c2[554],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[575],c2[574],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[576],c2[575],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[581],c2[580],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[607],c2[606],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[611],c2[610],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[615],c2[614],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[639],c2[638],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[644],c2[643],8),simde_mm_alignr_epi8(c2[644],c2[643],11)))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))));

//row: 4  [direct, 2 terms]
     d2[60]=simde_mm_xor_si128(c2[4],simde_mm_alignr_epi8(c2[44],c2[43],3));

//row: 6  [direct, 8 terms]
     d2[90]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[13],c2[12],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[182],c2[181],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[302],c2[301],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[337],c2[336],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[396],c2[395],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[523],c2[522],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[544],c2[543],4),simde_mm_alignr_epi8(c2[601],c2[600],13))))))));

//row: 7  [direct, 6 terms]
     d2[105]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[5],c2[4],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[40],c2[39],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[134],c2[133],1),simde_mm_xor_si128(c2[212],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[251],c2[250],6),simde_mm_alignr_epi8(c2[422],c2[421],2))))));

//row: 9  [direct, 8 terms]
     d2[135]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[11],c2[10],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[31],c2[30],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[313],c2[312],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[344],c2[343],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[392],c2[391],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[524],c2[523],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[546],c2[545],12),simde_mm_alignr_epi8(c2[613],c2[612],13))))))));

//row: 10  [direct, 6 terms]
     d2[150]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[38],c2[37],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[70],c2[69],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[125],c2[124],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[225],c2[224],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[248],c2[247],3),simde_mm_alignr_epi8(c2[426],c2[425],4))))));

//row: 12  [direct, 6 terms]
     d2[180]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[12],c2[11],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[44],c2[43],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[308],c2[307],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[342],c2[341],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[397],c2[396],2),c2[545])))));

//row: 14  [direct, 6 terms]
     d2[210]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[8],c2[7],12),simde_mm_xor_si128(c2[369],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[462],c2[461],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[486],c2[485],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[515],c2[514],8),simde_mm_alignr_epi8(c2[635],c2[634],12))))));

//row: 17  [direct, 5 terms]
     d2[255]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[12],c2[11],7),simde_mm_xor_si128(c2[427],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[487],c2[486],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[524],c2[523],11),simde_mm_alignr_epi8(c2[639],c2[638],1)))));

//row: 18  [direct, 5 terms]
     d2[270]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[42],c2[41],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[374],c2[373],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[402],c2[401],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[549],c2[548],15),simde_mm_alignr_epi8(c2[571],c2[570],14)))));

//row: 19  [direct, 5 terms]
     d2[285]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[12],c2[11],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[37],c2[36],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[220],c2[219],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[249],c2[248],10),simde_mm_alignr_epi8(c2[313],c2[312],4)))));

//row: 21  [direct, 5 terms]
     d2[315]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[43],c2[42],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[158],c2[157],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[494],c2[493],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[605],c2[604],1),simde_mm_alignr_epi8(c2[644],c2[643],8)))));

//row: 22  [direct, 4 terms]
     d2[330]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[2],c2[1],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[363],c2[362],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[398],c2[397],14),simde_mm_alignr_epi8(c2[522],c2[521],2))));

//row: 23  [direct, 4 terms]
     d2[345]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[42],c2[41],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[68],c2[67],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[308],c2[307],5),simde_mm_alignr_epi8(c2[553],c2[552],7))));

//row: 25  [direct, 4 terms]
     d2[375]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[32],c2[31],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[189],c2[188],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[211],c2[210],11),simde_mm_alignr_epi8(c2[432],c2[431],5))));

//row: 26  [direct, 4 terms]
     d2[390]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[11],c2[10],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[69],c2[68],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[131],c2[130],9),simde_mm_alignr_epi8(c2[457],c2[456],2))));

//row: 27  [direct, 3 terms]
     d2[405]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[41],c2[40],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[195],c2[194],8),simde_mm_alignr_epi8(c2[241],c2[240],9)));

//row: 28  [direct, 4 terms]
     d2[420]=simde_mm_xor_si128(c2[2],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[123],c2[122],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[583],c2[582],8),simde_mm_alignr_epi8(c2[643],c2[642],13))));

//row: 33  [direct, 4 terms]
     d2[495]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[43],c2[42],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[69],c2[68],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[341],c2[340],12),simde_mm_alignr_epi8(c2[635],c2[634],1))));

//row: 34  [direct, 4 terms]
     d2[510]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[11],c2[10],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[225],c2[224],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[459],c2[458],14),simde_mm_alignr_epi8(c2[522],c2[521],4))));

//row: 36  [direct, 4 terms]
     d2[540]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[1],c2[0],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[433],c2[432],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[453],c2[452],10),simde_mm_alignr_epi8(c2[547],c2[546],4))));

//row: 38  [direct, 4 terms]
     d2[570]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[8],c2[7],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[276],c2[275],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[302],c2[301],10),simde_mm_alignr_epi8(c2[369],c2[368],12))));

//row: 39  [direct, 4 terms]
     d2[585]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[38],c2[37],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[102],c2[101],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[221],c2[220],8),simde_mm_alignr_epi8(c2[574],c2[573],4))));

//row: 40  [direct, 3 terms]
     d2[600]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[1],c2[0],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[247],c2[246],7),simde_mm_alignr_epi8(c2[512],c2[511],14)));

//row: 41  [direct, 4 terms]
     d2[615]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[34],c2[33],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[102],c2[101],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[284],c2[283],7),simde_mm_alignr_epi8(c2[542],c2[541],8))));

//row: 45  [direct, 3 terms]
     d2[675]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[42],c2[41],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[188],c2[187],2),simde_mm_alignr_epi8(c2[306],c2[305],13)));

     if (i2>=1) {
       cm=&csimd[i2-1]; dm=&dsimd[i2-1];
       dm[15]=simde_mm_xor_si128(simde_mm_alignr_epi8(dm[1],dm[0],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[9],cm[8],7),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[45],cm[44],3),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[68],cm[67],14),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[99],cm[98],6),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[156],cm[155],4),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[186],cm[185],3),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[274],cm[273],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[315],cm[314],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[343],cm[342],13),simde_mm_xor_si128(cm[368],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[395],cm[394],11),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[459],cm[458],7),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[494],cm[493],9),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[554],cm[553],12),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[576],cm[575],10),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[607],cm[606],9),simde_mm_alignr_epi8(cm[639],cm[638],9))))))))))))))))));
       dm[30]=simde_mm_xor_si128(dm[15],simde_mm_xor_si128(dm[0],simde_mm_xor_si128(cm[6],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[75],cm[74],12),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[99],cm[98],8),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[134],cm[133],13),simde_mm_xor_si128(cm[158],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[216],cm[215],12),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[251],cm[250],12),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[274],cm[273],8),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[331],cm[330],11),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[372],cm[371],13),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[426],cm[425],15),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[456],cm[455],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[490],cm[489],9),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[516],cm[515],7),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[581],cm[580],3),simde_mm_alignr_epi8(cm[644],cm[643],8))))))))))))))))));
       dm[45]=simde_mm_xor_si128(simde_mm_alignr_epi8(dm[1],dm[0],1),simde_mm_xor_si128(cm[8],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[32],cm[31],7),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[101],cm[100],2),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[134],cm[133],12),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[183],cm[182],11),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[222],cm[221],10),simde_mm_xor_si128(cm[246],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[301],cm[300],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[344],cm[343],8),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[362],cm[361],6),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[392],cm[391],8),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[431],cm[430],7),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[493],cm[492],8),simde_mm_xor_si128(cm[512],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[555],cm[554],11),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[611],cm[610],12),simde_mm_alignr_epi8(cm[644],cm[643],11))))))))))))))))));
     }
  }

  // epilogue: kernel rows for the final 1 chunk(s), wrapping
  cm=&csimd[14]; dm=&dsimd[14];
  dm[15]=simde_mm_xor_si128(simde_mm_alignr_epi8(dsimd[0],dsimd[14],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[9],cm[8],7),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[45],cm[44],3),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[68],cm[67],14),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[99],cm[98],6),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[156],cm[155],4),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[186],cm[185],3),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[274],cm[273],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[315],cm[314],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[343],cm[342],13),simde_mm_xor_si128(cm[368],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[395],cm[394],11),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[459],cm[458],7),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[494],cm[493],9),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[554],cm[553],12),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[576],cm[575],10),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[607],cm[606],9),simde_mm_alignr_epi8(cm[639],cm[638],9))))))))))))))))));
  dm[30]=simde_mm_xor_si128(dsimd[29],simde_mm_xor_si128(dsimd[14],simde_mm_xor_si128(cm[6],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[75],cm[74],12),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[99],cm[98],8),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[134],cm[133],13),simde_mm_xor_si128(cm[158],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[216],cm[215],12),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[251],cm[250],12),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[274],cm[273],8),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[331],cm[330],11),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[372],cm[371],13),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[426],cm[425],15),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[456],cm[455],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[490],cm[489],9),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[516],cm[515],7),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[581],cm[580],3),simde_mm_alignr_epi8(cm[644],cm[643],8))))))))))))))))));
  dm[45]=simde_mm_xor_si128(simde_mm_alignr_epi8(dsimd[0],dsimd[14],1),simde_mm_xor_si128(cm[8],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[32],cm[31],7),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[101],cm[100],2),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[134],cm[133],12),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[183],cm[182],11),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[222],cm[221],10),simde_mm_xor_si128(cm[246],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[301],cm[300],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[344],cm[343],8),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[362],cm[361],6),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[392],cm[391],8),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[431],cm[430],7),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[493],cm[492],8),simde_mm_xor_si128(cm[512],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[555],cm[554],11),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[611],cm[610],12),simde_mm_alignr_epi8(cm[644],cm[643],11))))))))))))))))));

  // double P0..P3 so any rotation becomes one unaligned load
  for (int k=0;k<4;k++) {
    memcpy(kd+k*480,     d+k*240, 240);
    memcpy(kd+k*480+240, d+k*240, 240);
  }

  // stage 2: extension rows that reference the kernel
  for (i2=0; i2<15; i2++) {
     c2=&csimd[i2];
     d2=&dsimd[i2];
     const uint8_t *kb=kd+16*i2;

//row: 5  [P0<<180 + 6 sys]
     d2[75]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+180)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[1],c2[0],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[41],c2[40],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[93],c2[92],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[369],c2[368],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[494],c2[493],2),simde_mm_alignr_epi8(c2[642],c2[641],4)))))));

//row: 8  [P0<<153+P2<<38 + 7 sys]
     d2[120]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+153)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+998)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[12],c2[11],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[37],c2[36],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[99],c2[98],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[374],c2[373],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[492],c2[491],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[580],c2[579],1),simde_mm_alignr_epi8(c2[643],c2[642],7)))))))));

//row: 11  [P0<<165+P1<<107 + 5 sys]
     d2[165]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+165)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+587)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[3],c2[2],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[38],c2[37],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[369],c2[368],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[481],c2[480],1),simde_mm_alignr_epi8(c2[640],c2[639],8)))))));

//row: 13  [P1<<150 + 4 sys]
     d2[195]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+630)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[14],c2[13],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[102],c2[101],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[220],c2[219],10),simde_mm_alignr_epi8(c2[612],c2[611],2)))));

//row: 15  [P3<<49 + 5 sys]
     d2[225]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1489)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[3],c2[2],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[39],c2[38],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[314],c2[313],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[401],c2[400],13),simde_mm_alignr_epi8(c2[549],c2[548],14))))));

//row: 16  [P0<<205 + 4 sys]
     d2[240]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+205)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[35],c2[34],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[100],c2[99],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[336],c2[335],4),simde_mm_alignr_epi8(c2[601],c2[600],5)))));

//row: 20  [P0<<130 + 4 sys]
     d2[300]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+130)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[5],c2[4],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[102],c2[101],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[283],c2[282],11),simde_mm_alignr_epi8(c2[341],c2[340],7)))));

//row: 24  [P0<<58 + 4 sys]
     d2[360]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+58)),simde_mm_xor_si128(c2[2],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[102],c2[101],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[121],c2[120],2),simde_mm_alignr_epi8(c2[340],c2[339],12)))));

//row: 29  [P3<<103 + 3 sys]
     d2[435]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1543)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[45],c2[44],8),simde_mm_xor_si128(c2[422],simde_mm_alignr_epi8(c2[548],c2[547],6))));

//row: 30  [P2<<105 + 3 sys]
     d2[450]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1065)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[11],c2[10],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[313],c2[312],7),simde_mm_alignr_epi8(c2[392],c2[391],10))));

//row: 31  [P0<<175+P3<<108 + 2 sys]
     d2[465]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+175)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1548)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[35],c2[34],9),simde_mm_alignr_epi8(c2[220],c2[219],5))));

//row: 32  [P2<<211 + 3 sys]
     d2[480]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1171)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[7],c2[6],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[367],c2[366],14),simde_mm_alignr_epi8(c2[430],c2[429],7))));

//row: 35  [P0<<100 + 3 sys]
     d2[525]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+100)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[45],c2[44],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[191],c2[190],14),simde_mm_alignr_epi8(c2[370],c2[369],1))));

//row: 37  [P1<<161 + 2 sys]
     d2[555]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+641)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[34],c2[33],11),simde_mm_alignr_epi8(c2[403],c2[402],12)));

//row: 42  [P2<<71 + 2 sys]
     d2[630]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1031)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[14],c2[13],14),simde_mm_alignr_epi8(c2[131],c2[130],10)));

//row: 43  [P3<<125 + 3 sys]
     d2[645]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1565)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[32],c2[31],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[488],c2[487],15),simde_mm_alignr_epi8(c2[544],c2[543],1))));

//row: 44  [P0<<148 + 3 sys]
     d2[660]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+148)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[12],c2[11],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[224],c2[223],3),simde_mm_alignr_epi8(c2[282],c2[281],11))));
  }
}

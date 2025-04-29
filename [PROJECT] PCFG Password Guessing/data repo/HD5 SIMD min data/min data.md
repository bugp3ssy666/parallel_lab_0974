***不同并行度****的数据真实性证明，截图的是****最有利于提高SIMD加速比****的一组数据。*

***

### -no optimization -O2：
![4ed2c72751ef249fe1b1316fb7342fdf.png](./_resources/4ed2c72751ef249fe1b1316fb7342fdf.png)
2.95719 2.97881 2.93737 3.07604 2.9881

***

### SIMD*4 -O2：
![aa6a4ae72974fffa88a1ed083b599ef5.png](./_resources/aa6a4ae72974fffa88a1ed083b599ef5.png)
1.68335 1.67164 1.66344 1.68123 1.66313

***

### SIMD*2 -O2：
![04a8a89c7613ac91e2716de5851f1f0a.png](./_resources/04a8a89c7613ac91e2716de5851f1f0a.png)
3.14893 3.15891 3.13497 3.1324 3.13298

***

### SIMD*8 (-) -O2：
![c3fa7faebf772a67cabe07d0c4ef53fb.png](./_resources/c3fa7faebf772a67cabe07d0c4ef53fb.png)
1.20376 1.19794 1.20582 1.2002 1.19746

***

### SIMD*8 (+) -O2：
![60a61e9e2e9a6adb3d4c560823241d5d.png](./_resources/60a61e9e2e9a6adb3d4c560823241d5d.png)
1.18629 1.18236 1.18812 1.1833 1.18762 1.18942

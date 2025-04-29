*perf 分析结果参数波动较大，下面选择的是相对稳定有效的，每种算法两组数据。*

***

### SIMD*0：
	 Performance counter stats for 'bash test.sh 1 1':
	
	           118,433      cache-misses:u            #    1.019 % of all cache refs    
	        11,618,957      cache-references:u                                          
	           118,433      L1-dcache-load-misses:u   #    1.02% of all L1-dcache accesses
	        11,618,957      L1-dcache-loads:u                                           
	            38,120      LLC-load-misses:u         #   25.69% of all LL-cache accesses
	           148,390      LLC-loads:u                                                 
	
	      45.064750482 seconds time elapsed
	
	       0.052065000 seconds user
	       0.021373000 seconds sys
	
	 Performance counter stats for 'bash test.sh 1 1':
	
	           116,764      cache-misses:u            #    1.016 % of all cache refs    
	        11,493,488      cache-references:u                                          
	           116,764      L1-dcache-load-misses:u   #    1.02% of all L1-dcache accesses
	        11,493,488      L1-dcache-loads:u                                           
	            32,892      LLC-load-misses:u         #   22.18% of all LL-cache accesses
	           148,307      LLC-loads:u                                                 
	
	      40.068819178 seconds time elapsed
	
	       0.039272000 seconds user
	       0.037254000 seconds sys

***

### SIMD*2：
	 Performance counter stats for 'bash test.sh 1 1':
	
	           115,227      cache-misses:u            #    1.001 % of all cache refs    
	        11,512,144      cache-references:u                                          
	           115,227      L1-dcache-load-misses:u   #    1.00% of all L1-dcache accesses
	        11,512,144      L1-dcache-loads:u                                           
	            38,013      LLC-load-misses:u         #   26.25% of all LL-cache accesses
	           144,821      LLC-loads:u                                                 
	
	      40.064864733 seconds time elapsed
	
	       0.046523000 seconds user
	       0.025290000 seconds sys
	
	 Performance counter stats for 'bash test.sh 1 1':
	
	           116,554      cache-misses:u            #    1.012 % of all cache refs    
	        11,514,317      cache-references:u                                          
	           116,554      L1-dcache-load-misses:u   #    1.01% of all L1-dcache accesses
	        11,514,317      L1-dcache-loads:u                                           
	            37,126      LLC-load-misses:u         #   25.05% of all LL-cache accesses
	           148,195      LLC-loads:u                                                 
	
	      40.065777654 seconds time elapsed
	
	       0.034005000 seconds user
	       0.039277000 seconds sys

***

### SIMD*4： 
	 Performance counter stats for 'bash test.sh 1 1':
	
	           112,294      cache-misses:u            #    0.981 % of all cache refs    
	        11,443,967      cache-references:u                                          
	           112,294      L1-dcache-load-misses:u   #    0.98% of all L1-dcache accesses
	        11,443,967      L1-dcache-loads:u                                           
	            31,337      LLC-load-misses:u         #   21.99% of all LL-cache accesses
	           142,527      LLC-loads:u                                                 
	
	      35.062645654 seconds time elapsed
	
	       0.039089000 seconds user
	       0.031705000 seconds sys
		   
	 Performance counter stats for 'bash test.sh 1 1':
	
	           111,174      cache-misses:u            #    0.973 % of all cache refs    
	        11,420,291      cache-references:u                                          
	           111,174      L1-dcache-load-misses:u   #    0.97% of all L1-dcache accesses
	        11,420,291      L1-dcache-loads:u                                           
	            29,143      LLC-load-misses:u         #   20.26% of all LL-cache accesses
	           143,823      LLC-loads:u                                                 
	
	      35.064079936 seconds time elapsed
	
	       0.039263000 seconds user
	       0.032866000 seconds sys

***

### SIMD*8(-)：
	 Performance counter stats for 'bash test.sh 1 1':
	
	           114,978      cache-misses:u            #    0.999 % of all cache refs    
	        11,511,277      cache-references:u                                          
	           114,978      L1-dcache-load-misses:u   #    1.00% of all L1-dcache accesses
	        11,511,277      L1-dcache-loads:u                                           
	            29,201      LLC-load-misses:u         #   19.89% of all LL-cache accesses
	           146,812      LLC-loads:u                                                 
	
	      40.064853413 seconds time elapsed
	
	       0.043565000 seconds user
	       0.028940000 seconds sys
	
	 Performance counter stats for 'bash test.sh 1 1':
	
	           111,318      cache-misses:u            #    0.975 % of all cache refs    
	        11,416,839      cache-references:u                                          
	           111,318      L1-dcache-load-misses:u   #    0.98% of all L1-dcache accesses
	        11,416,839      L1-dcache-loads:u                                           
	            27,478      LLC-load-misses:u         #   19.33% of all LL-cache accesses
	           142,136      LLC-loads:u                                                 
	
	      35.062097372 seconds time elapsed
	
	       0.026986000 seconds user
	       0.042579000 seconds sys

***

### SIMD*8(+)：
	 Performance counter stats for 'bash test.sh 1 1':
	
	           114,855      cache-misses:u            #    0.997 % of all cache refs    
	        11,516,426      cache-references:u                                          
	           114,855      L1-dcache-load-misses:u   #    1.00% of all L1-dcache accesses
	        11,516,426      L1-dcache-loads:u                                           
	            35,057      LLC-load-misses:u         #   24.10% of all LL-cache accesses
	           145,486      LLC-loads:u                                                 
	
	      40.063256910 seconds time elapsed
	
	       0.027743000 seconds user
	       0.043150000 seconds sys
	
	 Performance counter stats for 'bash test.sh 1 1':
	
	           112,461      cache-misses:u            #    0.987 % of all cache refs    
	        11,392,099      cache-references:u                                          
	           112,461      L1-dcache-load-misses:u   #    0.99% of all L1-dcache accesses
	        11,392,099      L1-dcache-loads:u                                           
	            32,862      LLC-load-misses:u         #   22.96% of all LL-cache accesses
	           143,135      LLC-loads:u                                                 
	
	      35.059898167 seconds time elapsed
	
	       0.038277000 seconds user
	       0.029622000 seconds sys
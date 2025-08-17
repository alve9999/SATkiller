awk -F, 'NR>1 && $2<=19.8 && $3<=19.8 { my+=$2; mini+=$3; if ($4 != "YES") all="NO" } 
         END { if(all!="NO") all="YES"; print "Total MySAT Time:", my; print "Total MiniSAT Time:", mini; print "All match:", all }' ./results/summary.csv

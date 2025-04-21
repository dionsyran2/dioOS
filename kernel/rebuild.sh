echo "|------------------------ BUILDING APPS ---------------------------|"
cd ~/grubOS/applications
make all

echo 
echo 
echo "|---------------------- BUILDING KERNEL --------------------------|"
cd ~/grubOS/kernel
make kernel && make buildimg

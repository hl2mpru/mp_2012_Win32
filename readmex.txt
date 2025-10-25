Debian 12
apt-get install build-essential libstdc++-12-pic g++-multilib lib32gcc-s1

Debian 13
apt-get install build-essential libstdc++-14-pic g++-multilib lib32gcc-s1

cd game/server/
make -j 4 -f server_linux32_hl2mp.mak

Windows
Visual Studio 2012 Express Desktop
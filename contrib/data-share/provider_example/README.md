* `pip install -r requirements.txt`
* At the moment /home/ubuntu/Downloads/ is set as directory to upload .torrent files and store downloaded data.
* See settings.py
* So add Downloads dir manually.

#### Opentracker installing
 * `cvs` need to be installed, for debian based distros `sudo apt install cvs make gcc transmission-daemon`
 * `mkdir build && cd build`
 * `cvs -d :pserver:cvs@cvs.fefe.de:/cvs -z9 co libowfat`
 * `cd ..`
 * `git clone git://erdgeist.org/opentracker`
 * `cd opentracker`
 * `make`
 * copy  `opentracker.conf.sample` to `/usr/local/etc/opentracker.conf`, and edit config
 * copy `opentracker` file to `/usr/local/bin/opentracker`
 * And run server `$ opentracker -f /usr/local/etc/opentracker.conf`

#### Transmission client installing
 * `sudo apt install transmission-daemon`
 * `sudo systemctl transmission-daemon stop`
 * `sudo vim /etc/transmission-daemon/settings.json`
 * `sudo systemctl transmission-daemon start`  
 
 
#### Create paichain tx with data
`python create_paichain_tx_with_data.py -u 54.218.116.97:8888/file_api/download/71f2eff1-e70b-4ac9-bd66-d5de7f97354f/ -a MpDscYD8xp4MnDLW7WPsEkKVAnAbkUz5t4`
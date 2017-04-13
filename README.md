# xenia-smash

xenia-smash is a spin-off from the classic arcade game, Breakout, first invented by Steve Wozniak and friends in 1976. 

This game was designed to showcase the use of multi-core multithreading Realtime OS (RTOS) architecture.

## Installation

Hardware: Xilinx Zedboard Zynq-7000 Development Board

The game was developed using Xilinx's Vivado 2014.2. 

*Please note using a higher version of Vivado requires small software changes.*

1. Xilinx Vivado 2014.2 is downloadable at: http://www.xilinx.com/support/download/index.htm
2. After the downloaded file has been unzipped, launch xsetup.exe.
3. Click Next, and accept the agreements on subsequent pages.
4. When prompted to select edition to install, choose Vivado Design Edition. Click Next to continue.
5. At the installation customization, please select only the following:
	> 1. Design Tools > Vivado.
	> 2. Design Tools > Software Development Kit (SDK).
	> 3. Design Tools > DocNav is optional, only if you want the documentation to be installed locally (not very necessary in my opinion).
	> 4. Devices > uncheck every device except SoCs > Zynq-7000.
	> 5. Installation Options > Install Cable Drivers
	> 6. Installation Options > Enable WebTalk for Vivado to send usage statistics....
	> 7. Installation Options > Enable WebTalk for SDK to send usage statistics....
6. Click Next and Select a suitable directory, Next and Install. When the installation has completed, click Finish to close the wizard.
7. If you are presented with the Vivado License Manager window, just cancel it, as the built in Webpack license will suffice.


## Gameplay

xenia-smash is a multiplayer game, where the blue bar deflects the ball in a randomized upward trajectory (frenzy mode), while the yellow bar deflects the ball with respect to the incident angle. The yellow bar also possess the abilities of changing the speed and angle of deflection on particular segments of the bar itself. For every 10pts scored, the golden columns are randomized using a competing semaphores, as well as an increase in ball speed.

Demo : https://youtu.be/HY3XGmtBGRM

## Contributing

1. Fork it!
2. Create your feature branch: `git checkout -b my-new-feature`
3. Commit your changes: `git commit -am 'Add some feature'`
4. Push to the branch: `git push origin my-new-feature`
5. Submit a pull request :smile:

## History

*V1.0* 
> 1. Basic gameplay with 2 multi-colored bars (blue on frenzy mode)
> 2. Ball start towards a random upward trajectory
> 3. Game over screen 

## Credits

Basic Gameplay Specifications: https://wiki.nus.edu.sg/display/ee4214/Bricks+Breaker+Project+Specification

Installation Process: https://wiki.nus.edu.sg/display/ee4214/Installing+Xilinx+Vivado+2016.3


## License

MIT
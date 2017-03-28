# xenia-smash

Mid-project Requirements
* Main screen of game with all required components (Brick Zone, Score Zone)
* Ball and bar can move smoothly (update at 25 FPS)
	* Ball can move up and down continuously at 250 pixels/s
	* Bar can be controlled by player while ball is moving
	* Brick columns should be able to change color while ball and bar are moving

Extra features
* Pause
* Multiplayer?
* Special effects

Implementation
* 10 Brick-threads, must be stopped when no more bricks
* Tell ball if it has been hit
	- Update score when brick(s) hit
	- Two columns of golden bricks, worth 2 points per brick
* 1 ball-thread, sends location to brick-threads
* 1 score/progress thread
* 1 bar-thread
* 1 send-to-display thread (HIGHEST PRIO)
	- Mutex on globals???
* Display on MB1 (blocking wait)

Microblaze0
* Update states (compute)
* Send to MB1

Microblaze1
* Read state (blocking wait)
* Update display
* Init golden columns to 1 and 5

MB1
* Int[80] destroyed
* Int num_bricks_left

State update:
* Color/Restore golden bricks
  - Int[2] old_golden
  - Int[2] new_golden
* Remove destroyed bricks
  - Int[2] destroy
* Move ball
  - Int[2] old_pos
- Int[2] new_pos
* Move bar
  - Int bar_old_pos
  - Int bar_new_pos
* Update score
  - Int score
* Update current ball speed
  - Int speed
* If lose, show lose screen
  - Int lose

Brick Threads:
* Stop Thread when all bricks within thread are broken
* Message queue ball location from ball thread
* 2 Golden brick threads at a time
  - Use counting semaphore to implement imaginary golden-brick shared resource
  - Golden threads release semaphore (it's golden state) at scores of 10s


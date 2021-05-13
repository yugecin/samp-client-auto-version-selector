samp-client-auto-version-selector
=================================

Loads the correct version of the SA-MP client while the game is being launched.
(Specifically made for 0.3.7 and 0.3.DL)

SA-MP is a free Massively Multiplayer Online game mod for the PC version of
Rockstar Games Grand Theft Auto: San Andreas (tm) - https://sa-mp.com

No need to choose a version when connecting to a server - this does it for you.
Just pick a server in your favorite launcher and connect.

Use at your own risk - I am not responsible for your computer, dog or marriage.

installation
============

- install client 0.3.7 (https://www.sa-mp.com/download.php)
- copy <gamedir>/samp.dll into <gamedir>/samp-versions/0.3.7/samp.dlll
- install client 0.3.DL (No official download link available)
- copy <gamedir>/samp.dll into <gamedir>/samp-versions/0.3.DL/samp.dll
- replace <gamedir>/samp.dll with the samp.dll from this project

Then open the serverbrowser (or any samp launcher of your choice), and just
connect to a server.

(If you want the server browser for 0.3.7, just install 0.3.7 again,
or change the order of the steps above)

how does it work
================

When loaded, this custom samp.dll queries the server that is being connected to.
The server then hopefully responds with the server's rules, which should contain
the version. If that contains 'DL', the samp.dll from the samp-versions/0.3.DL
folder gets loaded. If anything fails, the 0.3.7 dll is loaded.

The only difference in files between 0.3.7 and 0.3.DL is the samp.dll file.
That makes the job of this switcher very easy, because it only needs to load
the correct samp.dll and does not need to mess with any files.

frequently questioned answers
=============================

Doesn't this way add a delay when launching the game?

Yes it does. But this way makes playing on two different versions very
convenient and I'm willing to compromise on that. You're most likely connecting
to a server that's online, and if the server is not located on Mars and has a
ping of over 1000, the delay should be reasonably short.
Note that this switcher waits for a maximum of 2500ms before giving up and
launching with 0.3.7.

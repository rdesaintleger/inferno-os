load std

# Window manager scenario on the headless backend: input through /dev/pointer,
# the screen read back from /dev/snapshot.ppm.  sleep takes whole seconds only.

fn click {
	echo m $1 $2 0 > /dev/pointer
	echo m $1 $2 1 > /dev/pointer
	echo m $1 $2 0 > /dev/pointer
}
fn shot {
	echo shot $1 `{md5sum /dev/snapshot.ppm}
}

wm/wm &
sleep 6
shot start
click 14 752			# open the menu
sleep 2
shot menu
echo m 30 626 0 > /dev/pointer	# hover "System >"
sleep 2
shot cascade
click 600 300			# click elsewhere: closes the menu
sleep 1
shot closed
i=1 2 3 4 5 6 7 8
for x in $i {			# open/close: window image set/destroy cycles
	click 14 752
	sleep 1
	click 600 300
	sleep 1
}
shot after
click 14 752
sleep 2
shot menu2
click 600 300
sleep 1
echo done

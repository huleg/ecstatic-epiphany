#!/bin/sh
exec ssh -p 11586 micah@localhost -N -L 7890:localhost:7890


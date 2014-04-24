#!/bin/sh
exec ssh ec2-user@pixelbees.net -f -N -L 11586:localhost:11586

#!/bin/sh
exec ssh -t -i ~/.ssh/aws-squarebot.pem ec2-user@54.196.4.174 -v -N -L 11586:localhost:11586
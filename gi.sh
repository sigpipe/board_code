echo "setting up github identity"
eval `ssh-agent -s`
ssh-add ~/.ssh/id2_rsa

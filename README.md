# DDS-control

## 3rd-party dependency

### protobuf

~~~~~~~~~~~~~~~~~
PROTOC_ZIP=protoc-3.8.0-osx-x86_64.zip
curl -OL https://github.com/google/protobuf/releases/download/v3.8.0/$PROTOC_ZIP
sudo unzip -o $PROTOC_ZIP -d /usr/local bin/protoc
sudo unzip -o $PROTOC_ZIP -d /usr/local include/*
rm -f $PROTOC_ZIP
~~~~~~~~~~~~~~~~~
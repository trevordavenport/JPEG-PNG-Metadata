JPEG-PNG-Metadata
=================

> Basically a homemade Exif Tool

```
Authors: Trevor Davenport + Ryan Erickson
```
#### Overview ####
```
  Memory Safe JPEG/PNG Metadata parser
  Determines validity of Image Files
  Checks for Malicious Code Entry / Buffer Overflows && Underflows
```
___

#### Memory Safe ####
Safe from the Following:
```
  1) array out-of-bounds writes, 
  2) array out-of-bounds reads, 
  3) out-of-bounds reads or writes to any buffer, 
  4) use of uninitialized data, accessing
  5) memory after it has been deallocated or is no longer valid, 
  6) freeing memory twice, and 
  7) freeing memory that was not allocated with malloc().
```
___

#### Tested Against ####
```
  Maliciously Constructed Image Files (i.e. Embedded with malware)
  Various forms of Malicious PNG/JPEG File.s
```
___

#### Breakdown of Images ####
Hexdump

![](http://i.imgur.com/8kk2RFo.png)


Examined Further

![](http://i.imgur.com/mJjrUIW.png)

Embedded Within Regular Images

![](http://i.imgur.com/QNGh8z2.png)

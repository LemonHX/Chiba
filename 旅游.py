import random

addresses = [
    "周庄古镇",
    "同里古镇", 
    "甪直古镇",
    "西塘古镇",
    "乌镇古镇",
    "南浔古镇",
    "朱家角古镇",
    "枫泾古镇",
    "新场古镇",
    "练塘古镇",
    "金泽古镇",
    "召稼楼古镇",
    "七宝古镇",
    "安昌古镇",
    "东浦古镇",
    "斗门古镇",
    "罗店古镇",
    "高桥古镇",
    "川沙古镇"
]

random.shuffle(addresses)
for address in range(0, 5):
    print(addresses[address])
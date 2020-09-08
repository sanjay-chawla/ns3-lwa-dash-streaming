#!/bin/bash 


CONTENT="../../DashWorkspace/bbb_$3s"
REQUESTS="output/$2/dualstripe/$1"
RESULTS="output/$2/dualstripe/$1/results"
DASH_REPORT="output/$2/dualstripe/$1/report.csv"
#FLOWMON="output/$2/dualstripe/$1/FlowMonitor.xml"
ADDRESSES="output/$2/dualstripe/$1/addresses"

QUANTILES=(0 5 10 25 50 75 90 95 100)

if [ -d $RESULTS ];
then
        rm -rf $RESULTS
        mkdir $RESULTS
else
        mkdir $RESULTS
fi

#printf "Node,RequestDuration,TCPOutputPacket,TCPOutputDelay,TCPOutputJitter,TCPOutputPloss,TCPInputPacket,TCPInputDelay,TCPInputJitter\n" >> $RESULTS/TcpStats.csv
printf "Node,StartUpDelay,AvgDownloadRate,StdDownloadRate,AvgBufferLevel,StdBufferLevel,StallEvents,RebufferingRatio,StallLabel,TotalStallingTime,AvgTimeStallingEvents,AvgVideoBitRate,AvgVideoQualityVariation,RSDVideoBitRate,AvgDownloadBitRate,Tinit,Frebuf,Trebuf,MOS\n" >> $RESULTS/DashStats.csv

min_rid=0

#Read requests
#source, server, startsAt, stopsAt, videoId, screenWidth, screenHeight
#UserId,StartsAt,StopsAt,VideoId,LinkCapacity,ScreenWidth,ScreenHeight
rid=0
while IFS='' read -r line || [[ -n "$line" ]]; do
    printf "${line}\n";
    if [[ !($line =~ ^#) ]] && (( $rid > $min_rid ));
    then
	# Retreive request data 
	fields=($(printf "%s" "$line"|cut -d',' --output-delimiter=' ' -f1-))
	
        source=${fields[0]}
	server=${fields[1]}
	startsAt=${fields[2]}
	stopsAt=${fields[3]}
	videoId=${fields[4]}
	sWidth=${fields[5]}
	sHeight=${fields[6]}
	printf "${source} ${videoId} ${sWidth}:${sHeight} \"$stopsAt\"-\"$startsAt\"\n"
	RequestDuration=$(echo "a" | awk '{printf stopsAt-startsAt}' stopsAt=$stopsAt startsAt=$startsAt)
	
	#########################################
	# Flow Monitor statistics
	#########################################
	# Retreive TCPOutputPacket, TCPOutputDelay, TCPOutputJitter, TCPOutputPloss 
	# Retreive TCPInputPacket, TCPInputDelay, TCPInputJitter, TCPInputPloss 
	# RequestDuration=$(echo "a" | awk '{printf '"$stopsAt"'-'"$startsAt"'}')
	# cat $FLOWMON | sed -s 's/\"/ /g' > $FLOWMON.mod
 	# awk 'BEGIN{ok=0}{if($1=="<Ipv4FlowClassifier>"){ok=1}else if($1=="</Ipv4FlowClassifier>"){ok=0}else if(ok==1){print $0} }' $FLOWMON | sed 's/\"/ /g' | awk '{print $3 " " $5 " " $7}' > $RESULTS/flowmonitor.flows
	# awk '{if(FILENAME~/addresses/){addr[$2]=$1}else{print $1 " " addr[$2] " " addr[$3] }}' $ADDRESSES $RESULTS/flowmonitor.flows > $RESULTS/flowmonitor.flows.id
	# # from client to server
	# awk '(($2=='"$source"')&&($3=='"$server"')){print $0}' $RESULTS/flowmonitor.flows.id > $RESULTS/flowmonitor.tmp
	# awk '{if(FILENAME~/tmp/){id=$1}else{ if($0~/<Flow flowId/){if(($3==id)&&($4=="timeFirstTxPacket=")){print $0}};}}' $RESULTS/flowmonitor.tmp $FLOWMON.mod | sed 's/\+//g' | sed 's/ns//g' > $RESULTS/flowmonitor.tmp2
	# TCPOutputPacket=$(awk '{printf $23}' $RESULTS/flowmonitor.tmp2)
	# TCPOutputDelay=$(awk '{printf $13/$25/1000000}' $RESULTS/flowmonitor.tmp2)
	# TCPOutputJitter=$(awk '{printf $15/$25/1000000}' $RESULTS/flowmonitor.tmp2)
	# TCPOutputPloss=$(awk '{printf ($23-$25)/$23}' $RESULTS/flowmonitor.tmp2)
    #     # from server to client
    #     awk '(($2=='"$server"')&&($3=='"$source"')){print $0}' $RESULTS/flowmonitor.flows.id > $RESULTS/flowmonitor.tmp
    #     awk '{if(FILENAME~/tmp/){id=$1}else{ if($0~/<Flow flowId/){if(($3==id)&&($4=="timeFirstTxPacket=")){print $0}};}}' $RESULTS/flowmonitor.tmp $FLOWMON.mod | sed 's/\+//g' | sed 's/ns//g' > $RESULTS/flowmonitor.tmp2
    #     TCPInputPacket=$(awk '{printf $23}' $RESULTS/flowmonitor.tmp2)
    #     TCPInputDelay=$(awk '{printf $13/$25/1000000}' $RESULTS/flowmonitor.tmp2)
    #     TCPInputJitter=$(awk '{printf $15/$25/1000000}' $RESULTS/flowmonitor.tmp2)
    #     TCPInputPloss=$(awk '{printf ($23-$25)/$23}' $RESULTS/flowmonitor.tmp2)	
    # 
	# printf "$source,$RequestDuration,$TCPOutputPacket,$TCPOutputDelay,$TCPOutputJitter,$TCPOutputPloss,$TCPInputPacket,$TCPInputDelay,$TCPInputJitter\n" >> $RESULTS/TcpStats.csv
	#####################################
	# DASH Player statistics
	#####################################
	# Time Node UserId  SegmentNumber SegmentRepID SegmentExperiencedBitrate(bit/s) BufferLevel(s)  StallingTime(msec) 
	# downloading speed (avg, dev), stall events, stall duration (avg, std), video bitrate (avg, std), quality variation (avg, std)
	videoId=$(($videoId+1))
        cat $CONTENT/dash_dataset_avc_bbb.csv | awk '(NR>4){print $0}' | sed 's/,/ /g' | awk '{print $1 " " $4}' > $RESULTS/representations	
	StartUpDelay=$(cat $DASH_REPORT | awk 'BEGIN{s=0};{if($2==source){if(ok!=1){s=$8;ok=1}}};END{printf s};' source=$source)
	AvgDownloadRate=$(cat $DASH_REPORT | awk '($2==source){s=s+$6;c++};END{if(c>0){printf s/c}else{printf 0}}' source=$source)
	StdDownloadRate=$(cat $DASH_REPORT | awk '($2==source){s=s+($6-AvgDownloadRate)^2;c++};END{if(c>0){printf sqrt(s/c)}else{printf 0}}' source=$source AvgDownloadRate=$AvgDownloadRate)
        AvgBufferLevel=$(cat $DASH_REPORT | awk '($2==source){s=s+$7;c++};END{if(c>0){printf s/c}else{printf 0}}' source=$source)
        StdBufferLevel=$(cat $DASH_REPORT | awk '($2==source){s=s+($7-AvgBufferLevel)^2;c++};END{if(c>0){printf sqrt(s/c)}else{printf 0}}' source=$source AvgBufferLevel=$AvgBufferLevel)
        StallEvents=$(cat $DASH_REPORT | awk 'BEGIN{c=0};{if($2==source){if(ok==1){if($8>0){c++}};ok=1}};END{printf c}' source=$source)
        TotalStallingTime=$(cat $DASH_REPORT | awk 'BEGIN{s=0};{if($2==source){if(ok==1){s=s+$8};ok=1}};END{printf s}' source=$source)
	RebufferingRatio=$(echo "a" | awk '{printf TotalStallingTime/1000/RequestDuration}' TotalStallingTime=$TotalStallingTime RequestDuration=$RequestDuration)
	StallLabel=$(echo "a" | awk '{m="NA";r=RebufferingRatio;if(r==0){m="NoStalling"}else if(r<=0.1){m="MildStalling"}else{m="SevereStalling"};printf m}' RebufferingRatio=$RebufferingRatio)
        AvgTimeStallingEvents=$(cat $DASH_REPORT | awk '($2==source){print $0}' | awk '{if(NR>1){s=s+$8;c++}};END{if(c>0){printf s/c}else{printf 0}}' source=$source)
	AvgVideoBitRate=$(awk '{if(!(FILENAME~/report/)){rep[$1]=$2}else{if($2==source){s=s+rep[$5];c++}}};END{if(c>0){printf s/c}else{printf 0}}' source=$source $RESULTS/representations $DASH_REPORT)
	RSDVideoBitRate=$(awk '{if(!(FILENAME~/report/)){rep[$1]=$2}else{if($2==source){s=s+(AvgVideoBitRate-rep[$5])^2;c++}}};END{if(c>0){printf sqrt(s/c)/AvgVideoBitRate*100}else{printf 0}}' AvgVideoBitRate=$AvgVideoBitRate AvgVideoBitRate=$AvgVideoBitRate source=$source $RESULTS/representations $DASH_REPORT)
	AvgVideoQualityVariation=$(awk 'function abs(x){return ((x < 0.0) ? -x : x)};BEGIN{br=0};{if(!(FILENAME~/report/)){rep[$1]=$2}else{if($2==source){if(br!=0){s=s+abs(br-rep[$5]);c++};br=rep[$5]}}};END{if(c>0){printf s/c}else {printf 0}}' source=$source $RESULTS/representations $DASH_REPORT)
	AvgDownloadBitRate=$(awk '{if($2==source){s=s+$6;c++}};END{if(c>0){printf s/c}else{printf 0}}' source=$source $DASH_REPORT)

	# QoS Metrics
	Tinit=$(echo "a" | awk '{m=3;d=StartUpDelay;if(d>0&&d<1000){m=1}else if(d>=1000&&d<5000){m=2}else{m=3};printf m}' StartUpDelay=$StartUpDelay)
	Frebuf=$(echo "a" | awk '{m=3;d=RebufferingRatio;if(d>=0&&d<0.02){m=1}else if(d>=0.02&&d<0.15){m=2}else{m=3};printf m}' RebufferingRatio=$RebufferingRatio)
	Trebuf=$(echo "a" | awk '{m=3;d=TotalStallingTime;if(d>=0&&d<5){m=1}else if(d>=5&&d<10){m=2}else{m=3};printf m}' TotalStallingTime=$TotalStallingTime)
	MOS=$(echo "a" | awk '{printf 4.23-0.0672*Tinit-0.742*Frebuf-0.106*Trebuf}' Tinit=$Tinit Frebuf=$Frebuf Trebuf=$Trebuf)
	printf "$source,$StartUpDelay,$AvgDownloadRate,$StdDownloadRate,$AvgBufferLevel,$StdBufferLevel,$StallEvents,$RebufferingRatio,$StallLabel,$TotalStallingTime,$AvgTimeStallingEvents,$AvgVideoBitRate,$AvgVideoQualityVariation,$RSDVideoBitRate,$AvgDownloadBitRate,$Tinit,$Frebuf,$Trebuf,$MOS\n" >> $RESULTS/DashStats.csv
    elif [[ !($line =~ ^#) ]];
    then
        # Retreive request data
        fields=($(printf "%s" "$line"|cut -d',' --output-delimiter=' ' -f1-))
        #source=${fields[1]}
        #server=${fields[2]}
        #startsAt=${fields[3]}
        #stopsAt=${fields[4]}
        #videoId=${fields[5]}
        #sWidth=${fields[6]}
        #sHeight=${fields[7]}
   
        
    fi
    rid=$((rid+1))
done < $REQUESTS/demo_req.csv










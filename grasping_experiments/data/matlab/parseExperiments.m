clear all; close all; clc;

dir_path='../bull_pile/';
bags=dir(dir_path);
bags(1:2)=[]; %remove the current/previous directories ('.' and '..')
prune=50; %remove the first steps of each trajectory (residual movements)
start_tol=0.01; %motion onset is detected if at least one joint crosses this velocity threshold

for i=1:numel(bags)
    %parse the joint states
    bagselect=rosbag(strcat(dir_path,bags(i).name));
    joint_states=readMessages(select(bagselect,'Topic','joint_states'));
    joint_states=joint_states(prune:end);
    n=numel(joint_states);
    
    data(i).joint_states.Name=joint_states{1}.Name;
    data(i).joint_states.Position=zeros(7,n);
    data(i).joint_states.Velocity=zeros(7,n);
    data(i).joint_states.Effort=zeros(7,n);
    data(i).joint_states.Travel=zeros(7,n);
    
    j_start=0;
    for j=1:n
        data(i).joint_states.Position(:,j)=joint_states{j}.Position;
        data(i).joint_states.Velocity(:,j)=joint_states{j}.Velocity;
        data(i).joint_states.Effort(:,j)=joint_states{j}.Effort;
        data(i).joint_states.t(j)=cast(joint_states{j}.Header.Stamp.Sec,'Double')+cast(joint_states{j}.Header.Stamp.Nsec,'Double')/1e9;
        if j>1
            data(i).joint_states.Travel(:,j)=data(i).joint_states.Travel(:,j-1)+abs(data(i).joint_states.Position(:,j)-data(i).joint_states.Position(:,j-1));
        end
        
        if (max(abs(joint_states{j}.Velocity)) > start_tol) && (j_start==0)
            j_start=j;
        end
    end
    
    %cut off the motionless initial part of the trajectories
    data(i).joint_states.Position(:,1:j_start-1)=[];
    data(i).joint_states.Velocity(:,1:j_start-1)=[];
    data(i).joint_states.Effort(:,1:j_start-1)=[];
    data(i).joint_states.Travel(:,1:j_start-1)=[];
    data(i).joint_states.t(1:j_start-1)=[];
    data(i).joint_states.t=data(i).joint_states.t-data(i).joint_states.t(1);
    
    %(gu)es(s)timate the perception time
  
    dff=diff(data(i).joint_states.t);
    dt=sum(dff)/numel(dff);
    data(i).pcpt_t=dt*(prune+j_start-1);
    data(i).mvmt_t=dt*(prune+n)-data(i).pcpt_t;
 
    plot(data(i).joint_states.t,data(i).joint_states.Velocity); grid on; hold on;
    
    %extract the cluster from the message
    result_cluster=readMessages(select(bagselect,'Topic','result_cluster'));
    %assuming that there is only one cluster in the bag
    mean(1,1)=result_cluster{1}.Points(1).X;
    mean(2,1)=result_cluster{1}.Points(1).Y;
    mean(3,1)=result_cluster{1}.Points(1).Z;
    
    if norm(mean) < 1e-5
        keyboard
    end    
    
    cov=zeros(3,3);
    for  j=1:3
        cov(1,j)=result_cluster{1}.Points(j+1).X;
        cov(2,j)=result_cluster{1}.Points(j+1).Y;
        cov(3,j)=result_cluster{1}.Points(j+1).Z;
    end
    
    data(i).result_cluster.mean=mean;
    data(i).result_cluster.cov=cov;
    
end

save(strcat(dir_path,'data.mat'),'data');

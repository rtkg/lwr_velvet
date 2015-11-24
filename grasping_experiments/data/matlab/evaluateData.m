clear all; close all; clc;

data_path{1}='../beer_square/data.mat';
data_path{2}='../beer_pile/data.mat';
data_path{3}='../cola_square/data.mat';
data_path{4}='../cola_pile/data.mat';
data_path{5}='../bull_square/data.mat';
data_path{6}='../bull_pile/data.mat';

for j=1:length(data_path)
load(data_path{j});
    n=length(data);
    res(j).M=zeros(2,n);
    for i=1:n
        res(j).pcpt_t(i)=data(i).pcpt_t;
        res(j).mvmt_t(i)=data(i).mvmt_t;
        res(j).sum_t(i)=data(i).pcpt_t+data(i).mvmt_t;
        res(j).T(i)=sum(data(i).joint_states.Travel(:,end)); %joint travel
        res(j).M(:,i)=data(i).result_cluster.mean(1:2);
        res(j).C(:,i)=sqrt(eig(data(i).result_cluster.cov(1:2,1:2)));
    end
  
      
res(j).c_std=sqrt(eig(cov(res(j).M'))); %standard deviation of the cluster means along the eigenvector directions
res(j).s_std_mean=mean(res(j).C'); %mean of the cluster standard deviations
res(j).s_std_std=std(res(j).C'); %std of the cluster standard deviations
    
end
M_total=[];
C_total=[];
T_total=[];
pcpt_t_total=[];
mvmt_t_total=[];
sum_t_total=[];
for j=1:length(data_path)
   M_total=[M_total res(j).M];
   C_total=[C_total res(j).C];
   T_total=[T_total res(j).T];
   pcpt_t_total=[pcpt_t_total res(j).pcpt_t];
   mvmt_t_total=[mvmt_t_total res(j).mvmt_t];
   sum_t_total=[sum_t_total res(j).sum_t];
end    
c_std_total=sqrt(eig(cov(M_total')));
s_std_mean_total=mean(C_total');

%%%%%%%%%%%%%%%% plot success rate for the soft grasping %%%%%%%%%%%%%%%%
succ=[22 3; 23 2; 25 0; 24 1; 25 0; 23 2];
f=figure; hold on; grid on;
font_size=10;
%subsampling for markers
%ind=linspace(1,100,10);
x_pos=linspace(0,5,6);
b=bar(x_pos,succ,0.5,'stacked');
b(1).FaceColor='g';
b(2).FaceColor='r';
h=legend('success', 'failure','Location','SouthEast');
set(h,'Interpreter','latex','FontSize',font_size);
ylabel('Nr. of experiments','interpreter','latex','FontSize',font_size);
%xlabel('Scenario','interpreter','latex','FontSize',font_size);
set(gca,'XTickLabel',{' '});
set(gca,'YTickLabel',{' '});
xlabels={'Beer rct.'; 'Beer rd.'; 'Coke rct.'; 'Coke rd.'; 'Bull rct.'; 'Bull rd.'};
ylabels={' 0'; ' 5'; '10'; '15'; '20'; '25'};

for i=1:length(xlabels)
    text(x_pos(i)-0.38,-1.8,xlabels{i},'Interpreter','Latex','fontsize',font_size);
end
for i=1:length(ylabels)
    text(-1.25,str2num(ylabels{i}),ylabels{i},'Interpreter','Latex','fontsize',font_size);
end
pbaspect([1.8,0.65,1]);
set(gcf,'PaperPositionMode','auto')
print(gcf,'success_soft_grasping','-dpdf','-r450');





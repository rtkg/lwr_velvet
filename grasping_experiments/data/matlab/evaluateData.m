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

%%%%%%%%%%%%%%%% plot success rates %%%%%%%%%%%%%%%%
succ=[99.2; 100; 77; 98.4; 88; 92; 100; 96; 100; 92];

f=figure; hold on; grid on;
font_size=10;
x_pos=[1:4 6:11];

b=bar(x_pos(1:4), succ(1:4),0.5); hold on;
b.FaceColor='g';
b=bar(x_pos(5:10), succ(5:10),0.5);
b.FaceColor='b';

h=ylabel('Success rate [\%]','interpreter','latex','FontSize',font_size);
h.Position=h.Position+[0.2 36 0];
set(gca,'XTickLabel',{' '});
set(gca,'YTickLabel',{' '});
ylabels={' 70'; ' 80'; ' 90'; '100'};
xlabels={'Box 1'; 'Box 2'; 'Spray'; 'Can'; 'Beer rct.'; 'Beer rd.'; 'Coke rct.'; 'Coke rd.'; 'Bull rct.'; 'Bull rd.'};

for i=1:length(xlabels)
    h=text(x_pos(i),68,xlabels{i},'Interpreter','Latex','fontsize',font_size);
    set(h, 'rotation', 60, 'HorizontalAlignment', 'right');
end
for i=1:length(ylabels)
    h=text(-0.1,str2num(ylabels{i}),ylabels{i},'Interpreter','Latex','fontsize',font_size);
    set(h, 'HorizontalAlignment', 'right');
end

ylim([70 100]);
pbaspect([1.8,0.4,1]);

set(gcf,'PaperPositionMode','auto')
print(gcf,'success_rates','-dpdf','-r450');




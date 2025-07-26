document.addEventListener('DOMContentLoaded', () => {
    const socket = io();
    
    // DOM 元素引用
    const totalTriggersCountEl = document.getElementById('total-triggers-count');
    const photoGalleryEl = document.getElementById('photo-gallery');
    const chartCanvas = document.getElementById('hourly-chart');

    // 全局图表实例
    let hourlyChart;
    
    // 1. 初始化图表
    function initChart() {
        const ctx = chartCanvas.getContext('2d');
        hourlyChart = new Chart(ctx, {
            type: 'bar',
            data: {
                // X轴标签：0点到23点
                labels: Array.from({ length: 24 }, (_, i) => `${i}:00`),
                datasets: [{
                    label: 'Trigger Count',
                    // 初始化数据为24个0
                    data: new Array(24).fill(0),
                    backgroundColor: 'rgba(66, 153, 225, 0.6)',
                    borderColor: 'rgba(66, 153, 225, 1)',
                    borderWidth: 1,
                    borderRadius: 5,
                }]
            },
            options: {
                responsive: true,
                maintainAspectRatio: false,
                scales: {
                    y: {
                        beginAtZero: true,
                        ticks: {
                            // 确保Y轴为整数
                            stepSize: 1
                        }
                    }
                },
                plugins: {
                    legend: {
                        display: false // 不显示图例
                    }
                }
            }
        });
    }

    // 2. 更新整个看板的函数
    function updateDashboard(events) {
        // 更新总次数
        totalTriggersCountEl.textContent = events.length;

        // 更新照片集 (清空后重新渲染)
        photoGalleryEl.innerHTML = '';
        events.forEach(event => addPhotoToGallery(event));

        // 更新图表
        updateChart(events);
    }
    
    // 3. 添加单张照片到画廊的函数
    function addPhotoToGallery(event) {
        const item = document.createElement('div');
        item.className = 'photo-item';
        item.innerHTML = `<img src="${event.image_url}" alt="Trigger photo">`;
        // prepend 使新照片显示在最前面
        photoGalleryEl.prepend(item);
    }

    // 4. 更新图表数据的函数
    function updateChart(events) {
        const hourlyCounts = new Array(24).fill(0);
        const today = new Date().toISOString().slice(0, 10); // 获取今天的日期 YYYY-MM-DD

        events.forEach(event => {
            // 只统计今天的数据
            if (event.timestamp.startsWith(today)) {
                const hour = new Date(event.timestamp).getHours();
                hourlyCounts[hour]++;
            }
        });
        
        hourlyChart.data.datasets[0].data = hourlyCounts;
        hourlyChart.update();
    }

    // --- WebSocket 事件监听 ---

    // 监听'initial_data'事件，用于页面首次加载
    socket.on('initial_data', (events) => {
        console.log('收到历史数据', events);
        updateDashboard(events);
    });

    // 监听'new_warning'事件，用于实时更新
    socket.on('new_warning', (event) => {
        console.log('收到新事件', event);
        // 实时更新总次数
        const newCount = parseInt(totalTriggersCountEl.textContent) + 1;
        totalTriggersCountEl.textContent = newCount;
        
        // 实时添加新照片
        addPhotoToGallery(event);

        // 实时更新图表
        const today = new Date().toISOString().slice(0, 10);
        if (event.timestamp.startsWith(today)) {
            const hour = new Date(event.timestamp).getHours();
            hourlyChart.data.datasets[0].data[hour]++;
            hourlyChart.update();
        }
    });
    
    // 初始化图表
    initChart();
});
# 回抽测试

回抽是将耗材拉回喷嘴的过程，用于防止空驶移动时的渗料和拉丝。回抽长度过短可能无法有效防止渗料，过长则可能导致堵料或欠挤出。PETG 和 TPU 等耗材更容易拉丝，因此与 PLA 或 ABS 相比可能需要更长的回抽长度。

此测试自动生成一个回抽塔。回抽塔是一个带有多个凹口的垂直结构，每个凹口以不同的回抽长度打印。打印完成后，我们可以检查塔的每一节，确定该耗材的最佳回抽长度。最佳回抽长度是能产生最干净塔身的最短长度。

![retraction_test](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/retraction/retraction_test.gif?raw=true)

![retraction_test_menu](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/retraction/retraction_test_menu.png?raw=true)

在对话框中，你可以选择起始和结束回抽长度以及回抽长度递增步长。默认值为：起始回抽长度 0mm、结束回抽长度 2mm、步长 0.1mm。这些值适用于大多数直驱挤出机。但对于远端（Bowden）挤出机，你可能希望将起始和结束回抽长度分别增大到 1mm 和 6mm，并将步长设为 0.2mm。

![retraction_test_print](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/retraction/retraction_test_print.jpg?raw=true)

> [!NOTE]
> 测试 PLA 或 ABS 等渗料极少的耗材时，回抽设置可能效果非常显著。你可能会发现回抽塔从一开始就很干净。这种情况下，在 OrcaSlicer 中将回抽长度设为 0.2mm - 0.4mm 就足够了。
> 反之，如果塔顶仍然有很多拉丝，建议先烘干耗材，并确认喷嘴安装正确、无泄漏。

> [!TIP]
> @ItsDeidara 制作了一个 HTML 工具帮助计算。如果公式让你头疼，可以看[这里](https://github.com/ItsDeidara/Orca-Slicer-Assistant)。

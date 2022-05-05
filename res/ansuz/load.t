<!doctype html>
<html>
	<head>
		<title>Ansuz</title>
		<meta charset="utf-8" />
		<link rel="stylesheet" href="/ansuz/bootstrap.min.css" />
		<link rel="stylesheet" href="/ansuz/bootstrap-utilities.min.css" />
		<style>
			{{css}}
		</style>
	</head>
	<body>
		<header><a href="/ansuz">ᚨᚾᛊᚢᛉ</a></header>
		<a href="/" id="home">ᚺᛖᛁᛗ</a>
		<main class="container">
			<table class="table text-white">
				<thead>
					<th scope="col-12">Plugin</th>
					<th scope="col-2"></th>
				</thead>
				<tbody>
				{% for plugin in plugins %}
					<tr>
						<td class="col-12">{{plugin.0}}</td>
						<td class="col-2"><a class="btn btn-primary" role="button" href="/ansuz/load/{{plugin.1}}">Load</a></td>
					</tr>
				{% endfor %}
				</tbody>
			</table>
		</main>
	</body>
</html>
